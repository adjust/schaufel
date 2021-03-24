#include <stdlib.h>
#include <string.h>

#include "test/test.h"
#include "utils/bintree.h"

void _free(Node n)
{
    free(n->data);
    free(n);
}

int main()
{
    void *binroot = NULL;
    Node n1, n2, n3;
    Node res = NULL;
    struct node n;  // stack allocated node for searching

    n1 = calloc(1,sizeof(*n1));
    n2 = calloc(1,sizeof(*n2));
    n3 = calloc(1,sizeof(*n3));
    n1->elem = 3;
    n1->data = strdup("huch");
    n1->free = &_free;
    n2->elem = 2;
    n2->data = strdup("moep");
    n2->free = &_free;
    n3->elem = 1;
    n3->data = strdup("hurz");
    n3->free = &_free;

    res = bin_search(&binroot,n1,&bin_intcomp);
    pretty_assert(res != NULL);
    pretty_assert(res->elem == 3);
    res = bin_search(&binroot,n2,&bin_intcomp);
    pretty_assert(res != NULL);
    pretty_assert(res->elem == 2);
    res = bin_search(&binroot,n3,&bin_intcomp);
    pretty_assert(res != NULL);
    pretty_assert(res->elem == 1);

    n.elem = 2;
    res = bin_find(&binroot,&n,&bin_intcomp);
    pretty_assert(strncmp("moep",(char *) res->data,4) == 0);

    // let bintree destroy all elements
    bin_destroy(binroot);
    return 0;
}
