#include <errno.h>
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
