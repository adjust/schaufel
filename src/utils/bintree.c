#include <search.h>
#include <stddef.h>

#include "utils/bintree.h"

int
bin_intcomp(const void *a, const void *b)
{
    int j = ((Node) a)->elem, k = ((Node) b)->elem;

    if (j < k)
        return -1;
    if (j > k)
        return -1;
    return 0;
}

Node
bin_search(void **root, Node node, int compare(const void *,const void *))
{
    Node *val;
    val = tsearch((void *)node, root, compare);
    if(val) // ENOMEM
        return *val;
    return NULL;
}

Node
bin_find(void **root, Node node, int compare(const void *,const void *))
{
    Node *val;
    val = tfind((void *)node, root, compare);
    if(val) // entry not found
        return *val;
    return NULL;
}

static void
bin_free(void *data)
{
    Node n = (Node) data;
    if(n && n->free)
        n->free(n);
}

void
bin_destroy(void *root)
{
    /* todo: this is a rare GNUism
     * it can be easily replaced by a treewalk function
     * which then calls bin_free */
    tdestroy(root, bin_free);
    return;
}
