#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "utils/htable.h"
#include "utils/scalloc.h"


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

typedef struct HTIter
{
    HTable     *ht;
    int32_t     slot;       /* current slot */
    HTNode     *node;       /* current node */
} HTIter;

HTable *
ht_create(uint16_t nslots, size_t data_size, HashFunc hfunc)
{
    size_t  size = sizeof(HTable) + nslots * sizeof(HTSlot);
    HTable *ht = (HTable *) SCALLOC(1, size);

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
        case HT_UPSERT:
            {
                HTNode   *new_node;

                if (found)
                    return action == HT_INSERT ? NULL : cur->data;

                /* allocate the node itself and also its data segment */
                new_node = SCALLOC(1, sizeof(HTNode) + ht->data_size);

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

HTIter *
ht_iter_create(HTable *ht)
{
    HTIter *iter = SCALLOC(1, sizeof(HTIter));

    iter->ht = ht;
    iter->slot = 0;
    iter->node = NULL;

    return iter;
}

const char *
ht_iter_next(HTIter *iter, void **data)
{
    HTNode *node = iter->node;
    bool    done = true;

    /* That was the last node in the list. Find the next nonempty slot */
    if (!node)
    {
        int i;

        for (i = iter->slot; i < iter->ht->nslots; ++i)
        {
            node = iter->ht->slots[i].list;

            if (node != NULL)
            {
                done = false;
                iter->slot = i + 1;

                break;
            }
        }
    }
    else
    {
        done = false;
    }

    /* No more slots in the hashtable */
    if (done)
        return NULL;

    iter->node = node->next;
    *data = node->data;
    return node->key;
}
