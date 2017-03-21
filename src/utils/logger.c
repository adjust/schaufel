#include <utils/logger.h>

static Logger logger;

void logger_init()
{
    Logger *l = &logger;
    l->fname  = "log/log";
    l->fd     = open(l->fname, O_CREAT | O_APPEND | O_WRONLY, 0777);
    if (l->fd < 0)
    {
        fprintf(stderr, "could not open logger fh: %s", strerror(errno));
        exit(1);
    }
    logger_log("logger initialized");
}

void logger_free()
{
    Logger *l = &logger;
    if (l->fd < 0)
        return;
    close(l->fd);
    l->fd = -1;
}

void logger_log(const char *fmt, ...)
{
    Logger *l = &logger;
    va_list args;
    //TODO: move alloc in init
    char *buf = calloc(LOG_BUFFER_SIZE + 2, sizeof *buf);
    int len = 0;
    buffer_set_timestamp(buf, len);
    va_start(args, fmt);
    len += vsnprintf(buf + len, LOG_BUFFER_SIZE, fmt, args);
    va_end(args);
    buf[len++] = '\n';
    logger_write(l->fd, buf, len);
    free(buf);
}
