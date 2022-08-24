#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test/test.h"
#include "utils/htable.h"

#define N 100

typedef struct
{
    HTableNode node;
    char expression[128];
    int value;
} ExpressionTableNodeData;

typedef ExpressionTableNodeData *ExpressionTableNode;

static bool
keyeq_func(const HTableNode* a_, const HTableNode* b_, void *arg)
{
    ExpressionTableNode a = (ExpressionTableNode)a_;
    ExpressionTableNode b = (ExpressionTableNode)b_;
    return (strcmp(a->expression, b->expression) == 0);
}

static uint32_t
hash_func(const HTableNode* a_, void *arg)
{
    ExpressionTableNode a = (ExpressionTableNode)a_;
    return htable_default_hash(a->expression, strlen(a->expression));
}

static void*
alloc_func(size_t size, void *arg)
{
    return malloc(size);
}

static void
free_func(void* mem, void *arg)
{
    free(mem);
}

static void
run_test(HTable* htable)
{
    int i, j;

    /* fill table */
    for(i = 1; i <= N; i++)
    {
        for(j = 1; j <= N; j++)
        {
            bool isNewNode;
            ExpressionTableNodeData new_node_data;
            sprintf(new_node_data.expression, "%d + %d", i, j);
            new_node_data.value = (i + j);
            htable_insert(htable, (HTableNode*)&new_node_data, &isNewNode);
            pretty_assert(isNewNode);
        }
    }

    pretty_assert(htable_nitems(htable) == (N*N));

    /* check hash table is filled right */
    for(i = 1; i <= N; i++)
    {
        for(j = 1; j <= N; j++)
        {
            ExpressionTableNode found_node;
            ExpressionTableNodeData query;
            sprintf(query.expression, "%d + %d", i, j);
            found_node = (ExpressionTableNode)htable_find(htable, (HTableNode*)&query);
            pretty_assert(found_node != NULL);
            pretty_assert(found_node->value == (i + j));
        }
    }

    /* try to delete a non-existing node */
    {
        bool result;
        ExpressionTableNodeData query;
        sprintf(query.expression, "ololo trololo");
        result = htable_delete(htable, (HTableNode*)&query);
        pretty_assert(result == false);
    }

    /* clean table */
    for(i = 1; i <= N; i++)
    {
        for(j = 1; j <= N; j++)
        {
            bool result;
            ExpressionTableNodeData query;
            sprintf(query.expression, "%d + %d", i, j);
            result = htable_delete(htable, (HTableNode*)&query);
            pretty_assert(result == true);
        }
    }

    pretty_assert(htable_nitems(htable) == 0);
}

int main()
{
    int i;
    HTable htable_data;

    htable_create(
            &htable_data,
            sizeof(ExpressionTableNodeData),
            hash_func,
            keyeq_func,
            alloc_func,
            free_func,
            NULL,
            NULL
        );

    run_test(&htable_data);
    htable_free_items(&htable_data);

    return 0;
}
