#include <utils/array.h>

typedef struct Array
{
    char   **payload;
    size_t   len;
    size_t   used;
} *Array;

Array
array_init(size_t len)
{
    Array array = calloc(1, sizeof(*array));
    if (len == 0)
        array->len = 1;
    else
        array->len = len;

    array->payload = calloc(len, sizeof(*(array->payload)));
    return array;
}

void
array_insert(Array array, char *val)
{
    if (array->used == array->len)
    {
        array->len *= 2;
        array->payload = realloc(array->payload, array->len * sizeof(*(array->payload)));
        for (uint32_t i = array->used + 1; i < array->len; ++i)
            array->payload[i] = NULL;
    }
    array->payload[array->used] = strdup(val);
    array->used += 1;
}

char *
array_pop(Array array)
{
    if (array->used == 0)
        return NULL;
    array->used -= 1;
    return array->payload[array->used];
}

void
array_free(Array *array)
{
    for (uint32_t i = 0; i < (*array)->len; ++i)
        free((*array)->payload[i]);
    free((*array)->payload);
    free(*array);
}
