#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "utils/htable.h"


#define MAX_KEY_LEN 128

typedef uint8_t byte;
typedef struct HTNode HTNode;

typedef struct HTNode
{
    char        key[MAX_KEY_LEN];
    HTNode     *next;
    byte        data[];
} HTNode;

typedef struct HTSlot
{
    HTNode     *list;
} HTSlot;

typedef struct HTable
{
    HashFunc    hashfunc;
    size_t      data_size;  /* size of data segment of each node */
    uint16_t    nslots;
    HTSlot      slots[];
} HTable;

HTable *
ht_create(uint16_t nslots, size_t data_size, HashFunc hfunc)
{
    size_t  size = sizeof(HTable) + nslots * sizeof(HTSlot);
    HTable *ht = (HTable *) malloc(size);

    memset(ht, 0, size);
    ht->nslots = nslots;
    ht->hashfunc = hfunc;
    ht->data_size = data_size;

    return ht;
}

void *
ht_search(HTable *ht, const char *key, HTAction action)
{
    int         hash;
    HTSlot     *slot;
    HTNode     *cur;
    HTNode     *prev = NULL;
    bool        found = false;

    hash = ht->hashfunc(key);
    slot = &ht->slots[hash % ht->nslots];

    cur = slot->list;
    if (cur)
    {
        while (1)
        {
            if (strncmp(key, cur->key, MAX_KEY_LEN) == 0)
            {
                found = true;
                break;
            }

            if (cur->next == NULL)
                break;

            prev = cur;
            cur = cur->next;
        }
    }

    switch (action)
    {
        case HT_FIND:
            return found ? cur->data : NULL;
        case HT_INSERT:
            {
                HTNode   *new_node;

                if (found)
                    return NULL;

                /* allocate the node itself and also its data segment */
                new_node = malloc(sizeof(HTNode) + ht->data_size);

                strncpy(new_node->key, key, MAX_KEY_LEN);
                new_node->key[MAX_KEY_LEN - 1] = '\0';
                new_node->next = slot->list;
                slot->list = new_node;

                return new_node->data;
            }
        case HT_REMOVE:
            if (found)
            {
                if (prev)
                    prev->next = cur->next;
                else
                    slot->list = cur->next;
                free(cur);
            }
            return NULL;
    }

    /* shouldn't get here, but to keep compiler quiet */
    return NULL;
}

