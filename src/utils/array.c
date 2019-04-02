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
    if (!array) {
        logger_log("%s %d calloc failed\n", __FILE__, __LINE__);
        abort();
    }
    if (len == 0)
        array->len = 1;
    else
        array->len = len;

    array->payload = calloc(len, sizeof(*(array->payload)));
    if (!array->payload) {
        logger_log("%s %d calloc failed\n", __FILE__, __LINE__);
        abort();
    }
    return array;
}

size_t
array_used(Array array)
{
    if (array == NULL)
        return 0;

    return array->used;
}

char *
array_get(Array array, size_t index)
{
    if (array == NULL)
        return NULL;

    if (index > array->used - 1)
        return NULL;

    return array->payload[index];
}

void
array_insert(Array array, char *val)
{
    if (array == NULL)
        return;

    if (array->used == array->len)
    {
        array->len *= 2;
        array->payload = realloc(array->payload, array->len * sizeof(*(array->payload)));
        if (!array->payload) {
            logger_log("%s %d realloc failed\n", __FILE__, __LINE__);
            abort();
        }
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
    if (!array)
       return;
    if (!*array)
       return;
    for (uint32_t i = 0; i < (*array)->len; ++i)
        free((*array)->payload[i]);
    free((*array)->payload);
    free(*array);
    *array = NULL;
}

