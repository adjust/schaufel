#include <search.h>
#include <stddef.h>

#include "utils/bintree.h"


/*
 * bin_intcomp
 *      standard comparison function for bintree
 *      compares integers in Node data structure
 *      use this as a compare functionin bin_{search,find}
 */
int
bin_intcomp(const void *a, const void *b)
{
    int j = ((Node) a)->elem, k = ((Node) b)->elem;

    if (j < k)
        return -1;
    if (j > k)
        return 1;
    return 0;
}

/*
 * bin_search
 *      find or insert node in binary tree (using compare function)
 */
Node
bin_search(void **root, Node node, int compare(const void *,const void *))
{
    Node *val;
    val = tsearch((void *)node, root, compare);
    if(val) // ENOMEM
        return *val;
    return NULL;
}

/*
 * bin_find
 *      find node in binary tree (using compare function)
 */
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
