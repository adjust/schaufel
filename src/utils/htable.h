/*
 * htable.h
 * (c) Alexandr A Alexeev 2011-2016 | http://eax.me/
 */
#ifndef _SCHAUFEL_UTIL_HTABLE_H_
#define _SCHAUFEL_UTIL_HTABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct HTableNode
{
    struct HTableNode* next;    /* next node in a chain, if any */
    uint32_t hash;              /* hash function value */
} HTableNode;

typedef uint32_t (*htable_hash_func) (const HTableNode* a, void *arg);
typedef bool (*htable_keyeq_func) (const HTableNode* a, const HTableNode* b, void *arg);
typedef void* (*htable_alloc_func) (size_t size, void *arg);
typedef void (*htable_free_func) (void* mem, void *arg);
typedef void (*htable_before_node_free_func) (HTableNode* node, void *arg);

/* Hash table representation */
typedef struct {
    HTableNode** items; /* table items */
    size_t nitems;      /* how many items are stored in hash table */
    uint32_t mask;      /* mask, aplied to hash function */
    size_t size;        /* current hash table size */

    /* user-specified arguments */

    size_t node_size;
    htable_hash_func hfunc;
    htable_keyeq_func eqfunc;
    htable_alloc_func allocfunc;
    htable_free_func freefunc;
    htable_before_node_free_func bnffunc;
    void* arg;
} HTable;

extern uint32_t htable_default_hash(const char *key, const size_t key_len);
extern void htable_create(
        HTable* tbl,
        size_t node_size,
        htable_hash_func hfunc,
        htable_keyeq_func eqfunc,
        htable_alloc_func allocfunc,
        htable_free_func freefunc,
        htable_before_node_free_func bnffunc,
        void* arg
    );
extern void htable_free_items(HTable* tbl);
extern HTableNode* htable_find(HTable* tbl, HTableNode* query);
extern void htable_insert(HTable* tbl, HTableNode* node, bool* isNewNode);
extern bool htable_delete(HTable* tbl, HTableNode* query);

#define htable_nitems(tbl) ((tbl)->nitems)

#endif
