#ifndef _SCHAUFEL_UTILS_ARRAY_H
#define _SCHAUFEL_UTILS_ARRAY_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utils/logger.h>
#include <utils/options.h>

Array array_init(size_t len);

size_t array_used(Array array);

char * array_get(Array array, size_t index);

void array_insert(Array array, char *val);

char * array_pop(Array array);

void array_free(Array *array);

#endif
