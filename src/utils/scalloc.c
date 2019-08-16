#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "utils/logger.h"
#include "utils/scalloc.h"


void *
scalloc(size_t n, size_t s, char *file, size_t line)
{
    void *ret = calloc(n,s);
    if (!ret) {
        if(get_logger_state())
            logger_log("%s %lu: Failed to calloc: %s\n", file, line,
            strerror(errno));
        else
            fprintf(stderr, "%s %lu: Failed to calloc: %s\n", file, line,
            strerror(errno));
        abort();
    }
    return ret;
}

/*
 * ssprintf
 *      Allocates the exact amount of memory for the formatted string and
 *      returns formatted string. Returned memory must be released manually
 *      by calling SFREE or free.
 */
char *
ssprintf(const char *fmt, char *file, size_t line, ...)
{
    va_list     args1, args2;
    size_t      len;
    char       *str;

    va_start(args1, line);
    va_copy(args2, args1);
    len = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);

    /* +1 for terminal \0 symbol */
    str = scalloc(1, len + 1, file, line);
    vsprintf(str, fmt, args2);
    va_end(args2);

    return str;
}
