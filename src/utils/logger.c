#include <utils/logger.h>

static Logger logger;

void logger_init(const char *fname)
{
    Logger *l = &logger;
    l->fname  = strdup(fname);
    l->fd     = open(l->fname, O_CREAT | O_APPEND | O_WRONLY, 0777);
    if (l->fd < 0)
    {
        fprintf(stderr, "could not open logger fh: %s", strerror(errno));
        exit(1);
    }

    l->buf = calloc(LOG_BUFFER_SIZE + 2, sizeof *(l->buf));
    logger_log("logger initialized");
}

void logger_free()
{
    Logger *l = &logger;
    if (l->fd < 0)
        return;
    free(l->fname);
    free(l->buf);
    close(l->fd);
    l->fd = -1;
}

void logger_log(const char *fmt, ...)
{
    Logger *l = &logger;
    va_list args;
    int len = 0;
    buffer_set_timestamp(l->buf, len);
    va_start(args, fmt);
    len += vsnprintf(l->buf + len, LOG_BUFFER_SIZE - len, fmt, args);
    va_end(args);
    if (len > LOG_BUFFER_SIZE)
        len = LOG_BUFFER_SIZE;
    l->buf[len++] = '\n';
    l->buf[len++] = '\0';
    logger_write(l->fd, l->buf, len);
}
