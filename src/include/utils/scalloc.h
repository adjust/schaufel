#ifndef _SCHAUFEL_UTILS_SCALLOC_H
#define _SCHAUFEL_UTILS_SCALLOC_H


void *scalloc(size_t n, size_t s, char *file, size_t line);
#define SCALLOC(n, s) scalloc(( n), (s), __FILE__, __LINE__)

#define SFREE(e) \
    do { \
        free(e); \
        e = NULL; \
    } while (0)

#endif
