#include <utils/logger.h>

static int log_fd;
static char *log_fname;
static _Thread_local char log_buffer[LOG_BUFFER_SIZE + 2];

void logger_init(const char *fname)
{
    log_buffer[0] = '\0';
    log_fname     = strdup(fname);
    if (!log_fname)
    {
        fprintf(stderr, "failed to allocate memory: %s\n", strerror(errno));
        exit(1);
    }
    log_fd        = open(fname, O_CREAT | O_APPEND | O_WRONLY, 0640);
    if (log_fd < 0)
    {
        fprintf(stderr, "could not open logger fh: %s", strerror(errno));
        exit(1);
    }

    logger_log("logger initialized");
}

void logger_free()
{
    if (log_fd < 0)
        return;
    free(log_fname);
    close(log_fd);
    log_buffer[0] = '\0';
    log_fd = -1;
}

static size_t buffer_set_timestamp(char *buf)
{
    size_t len;
    struct tm *local;
    time_t tm = time(NULL);
    local = localtime(&tm);
    len = strftime(buf, LOG_BUFFER_SIZE, "%a %b %e %T %Y ", local);
    if (len == 0)
        buf[0] = '\0';
    return len;
}

static void logger_write(int fd, char *buf, size_t len)
{
    if (len > LOG_BUFFER_SIZE + 2)
        len = LOG_BUFFER_SIZE + 2;
    if (write(fd, buf, len) < 0)
        fprintf(stderr, "while writing to logfile %s", strerror(errno));
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
    logger_write(log_fd, log_buffer, len);
}
