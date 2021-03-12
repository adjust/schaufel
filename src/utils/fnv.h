#ifndef _SCHAUFEL_UTILS_FNV_H
#define _SCHAUFEL_UTILS_FNV_H
typedef uint32_t Fnv32_t;

Fnv32_t (*fnv_init(char *name)) (void *,size_t);
Fnv32_t (*fold_init(char *name)) (Fnv32_t);

#endif
