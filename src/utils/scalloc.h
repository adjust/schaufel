#ifndef _SCHAUFEL_UTILS_SCALLOC_H
#define _SCHAUFEL_UTILS_SCALLOC_H

#include "logger.h"
/*
#define SCALLOC(r, n, s) \
        if (!((r) = calloc((n),sizeof(s)))) { \
            logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, \
            strerror(errno)); \
            abort(); \
        }
*/

void *scalloc(size_t n, size_t l, char *file, size_t line);
#define SCALLOC(n, s) scalloc(( n), (s), __FILE__, __LINE__);
#define SFREE(e) \
    free(e); \
    e = NULL;


#endif
