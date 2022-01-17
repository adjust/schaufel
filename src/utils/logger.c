#include "schaufel.h"
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <regex.h>

#include "utils/config.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/strlwr.h"

#define SYSLOG_NAMES 1
#include <syslog.h>

static volatile atomic_bool logger_state = false;
static _Thread_local char log_buffer[LOG_BUFFER_SIZE + 2];

static struct {
    void (*log)(char *, size_t);
    void (*free)(void);
    int loglevel;
    int fd;
    int facility;
    bool timestamp;
} logger;

static void log_fd(char *, size_t);
static void log_null(char *, size_t);
static void log_syslog(char *, size_t);
static void log_syslog_free(void);

bool logger_validate(config_setting_t *config)
{
    const char *retval = NULL;
    char *type = NULL;

    if(!CONF_L_IS_STRING(config, "type", &retval,
        "logger: need a type (file/stdout/stderr/syslog/null)"))
        goto error;

    // convert to lowercase
    type = strdup(retval);
    if(type == NULL)
    {
        fprintf(stderr, "%s %d: failed allocate!\n",
            __FILE__, __LINE__);
        goto error;
    }
    type = strlwr(type);

    size_t len = strnlen(type,20);
    if (len == 4 && strncmp(type,"file",4) == 0)
    {
        // set standard mode
        mode_t m = 0640;
        config_setting_lookup_int(config, "mode", (int *) &m);
        if (m & (S_ISVTX | S_ISGID | S_ISUID))
        {
            fprintf(stderr,
                "%s %d: Cowardly refusing to create file mode %04o\n",
                __FILE__, __LINE__, m);
            goto error;
        }
        if (m > 07777)
        {
            fprintf(stderr,
                "%s %d: Illegal mode: %o\n",
                __FILE__, __LINE__, m);
            goto error;
        }

        if(config_setting_lookup_string(config, "file", &retval)
            != CONFIG_TRUE)
        {
            fprintf(stderr, "%s, %d: missing configuration for type: "
                "%s\n", __FILE__, __LINE__, retval);
            goto error;
        }
    }
    else if (len == 6 && strncmp(type,"stdout",6) == 0)
        (void) true;
    else if (len == 6 && strncmp(type,"stderr",6) == 0)
        (void) true;
    else if (len == 4 && strncmp(type,"null",4) == 0)
        (void) true;
    else if (len == 6 && strncmp(type,"syslog",6) == 0)
    {
        const char *facility = NULL, *ident = "schaufel";
        config_setting_t *setting = config_setting_get_member(
            config, "facility");

        if (setting == NULL)
        {
            // set default facility to daemon
            setting = config_setting_add(config, "facility",
                CONFIG_TYPE_STRING);
            config_setting_set_string(setting, "daemon");
        }
        else if (config_setting_lookup_string(
            config, "facility", &facility) == CONFIG_TRUE)
        {
            facility = config_setting_get_string(setting);
            // TODO:
            // facilitynames needs _BSD_SOURCE//_GNU_SOURCE
            int i = 0, res;

            do {
                res = facilitynames[i].c_val;
                if (strcmp(facilitynames[i].c_name,facility) == 0)
                    break;
            } while(facilitynames[++i].c_val > -1);

            if (res == -1)
            {
                fprintf(stderr, "%s %d: unknown syslog facility %s\n",
                    __FILE__, __LINE__, facility);
                goto error;
            }
        }
        else
        {
            fprintf(stderr, "%s %d: expected logger facility to be"
                " of type string\n", __FILE__, __LINE__);
            goto error;
        }

        setting = config_setting_get_member(config, "ident");
        if (setting == NULL)
        {
            setting = config_setting_add(config, "ident", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, ident);
        }
        else if (config_setting_lookup_string(config, "ident", &ident)
            != CONFIG_TRUE)
        {
            fprintf(stderr, "%s %d: syslog ident must be a string \n",
                __FILE__, __LINE__);
            // todo: limits on syslogs ident?
            goto error;
        }
    }
    else
    {
        fprintf(stderr, "%s %d: unsupported type %s\n",
            __FILE__, __LINE__, type);
        goto error;
    }

    free(type);

    return true;

    error:
    free(type);
    return false;
}

bool get_logger_state()
{
    return get_state(&logger_state);
}

void logger_init(config_setting_t *config)
{
    const char *fname = NULL, *type;
    mode_t mode = 0640;

    // all loggers except syslog need a timestamp
    memset((void *) &logger, 0, sizeof(logger));
    logger.timestamp = true;

    config_setting_lookup_string(config,"type",&type);
    if (strncmp(type, "file", 4) == 0)
    {
        config_setting_lookup_string(config,"file",&fname);
        config_setting_lookup_int(config,"mode",(int *) &mode);
        logger.fd = open(fname, O_CREAT | O_APPEND | O_WRONLY, mode);

        if (logger.fd < 0)
        {
            fprintf(stderr, "could not open logger fh: %s", strerror(errno));
            exit(1);
        }
        logger.log = &log_fd;
    }
    else if (strncmp(type, "stderr", 6) == 0)
    {
        logger.fd = fileno(stderr);
        logger.log = &log_fd;
    }
    else if (strncmp(type, "stdout", 6) == 0)
    {
        logger.fd = fileno(stdout);
        logger.log = &log_fd;
    }
    else if (strncmp(type, "null", 6) == 0)
    {
        logger.log = &log_null;
    }
    else if (strncmp(type, "syslog", 6) == 0)
    {
        // todo, facility
        const char *facility = NULL, *ident = NULL;

        // validator guarantees these exist
        config_setting_lookup_string(config, "facility", &facility);
        config_setting_lookup_string(config, "ident", &ident);


        if (facility == NULL || ident == NULL)
        {
            fprintf(stderr, "%s %d: expected facility and ident\n",
                __FILE__, __LINE__);
            exit(1);
        }

        int i = 0, res;

        do {
            res = facilitynames[i].c_val;

            if (strcmp(facilitynames[i].c_name,facility) == 0)
                break;
        } while (facilitynames[i++].c_val > -1);

        if (res == -1)
        {   // paranoia error handling
            fprintf(stderr, "%s %d: facility \"%s\" does not exist",
                __FILE__, __LINE__, facility);
            exit(1);
        }

        // use LOG_CONS to log to console if syslog is unavailable
        openlog(ident, LOG_PID | LOG_CONS, res);
        logger.log = &log_syslog;
        logger.free = &log_syslog_free;
        logger.timestamp = false;
    }
    else
    {
        fprintf(stderr, "no such logger type: %s", type);
        exit(1);
    }
    log_buffer[0] = '\0';

    set_state(&logger_state, true);

    logger_log("logger initialized");
}

void logger_free()
{
    if (logger.fd < 0)
        return;
    if(logger.free)
        logger.free();
    close(logger.fd);
    log_buffer[0] = '\0';
    logger.fd = -1;
}

static size_t buffer_set_timestamp(char *buf)
{
    size_t len;
    struct tm *local;
    time_t tm = time(NULL);

    if (logger.timestamp == false)
    {
        buf[0] = '\0';
        return 0;
    }

    local = localtime(&tm);
    len = strftime(buf, LOG_BUFFER_SIZE, "%a %b %e %T %Y ", local);
    if (len == 0)
        buf[0] = '\0';

    return len;
}

static void log_fd(char *buf, size_t len)
{
    if (len > LOG_BUFFER_SIZE + 2)
        len = LOG_BUFFER_SIZE + 2;

    if (write(logger.fd, buf, (len-1)) < 0)
        fprintf(stderr, "while writing to logfile %s", strerror(errno));
}

static void log_syslog(char *buf, UNUSED size_t len)
{
    syslog(LOG_INFO | LOG_USER, "%s", buf);
}

static void log_syslog_free(void)
{
    closelog();
}

static void log_null(UNUSED char *buf, UNUSED size_t len)
{
    return;
}

void logger_log(const char *fmt, ...)
{
    va_list args;
    int len = buffer_set_timestamp(log_buffer);
    va_start(args, fmt);
    len += vsnprintf(log_buffer + len, LOG_BUFFER_SIZE - len, fmt, args);
    va_end(args);
    if (len > LOG_BUFFER_SIZE)
        len = LOG_BUFFER_SIZE;
    log_buffer[len++] = '\n';
    log_buffer[len++] = '\0';
    logger.log(log_buffer, len);
}
