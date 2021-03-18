#ifndef _SCHAUFEL_UTILS_BINTREE_H
#define _SCHAUFEL_UTILS_BINTREE_H
#include <stdint.h>

typedef struct node *Node;
typedef struct node {
    uint32_t elem;
    void    *data;
    void   (*free) (Node);
} *Node;

int bin_intcomp(const void *a, const void *b);
Node bin_search(void **root, Node node, int compare(const void *,const void *));
Node bin_find(void **root, Node node, int compare(const void *,const void *));
void bin_destroy(void *root);

#endif
