#ifndef _SCHAUFEL_LOGGER_H_
#define _SCHAUFEL_LOGGER_H_

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LOG_BUFFER_SIZE 10000

typedef struct Logger
{
    char *fname;
    int fd;
} Logger;

void logger_init(const char* fname);
void logger_free();
void logger_log(const char *fmt, ...);

#define buffer_set_timestamp(_buf, _len)                         \
    do {                                                         \
        char *_timestr;                                          \
        struct tm *_local;                                       \
        time_t _time = time(NULL);                               \
        _local = localtime(&_time);                              \
        _timestr = asctime(_local);                              \
        _len += snprintf(_buf, LOG_BUFFER_SIZE, "%s", _timestr); \
        _buf[_len - 1] = ' ';                                    \
    } while (0)

#define logger_write(_fd, _buf, _len)          \
    do {                                       \
        if (_len > LOG_BUFFER_SIZE)            \
            _len = LOG_BUFFER_SIZE;            \
        if (write(_fd, _buf, _len) < 0)        \
            fprintf(                           \
                stderr,                        \
                "while writing to logfile %s", \
                strerror(errno)                \
            );                                 \
    } while(0)

#endif
