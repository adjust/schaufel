#include <errno.h>
#include <stdint.h>
#include <utils/config.h>
#include <utils/scalloc.h>
#include "hooks.h"

typedef struct hooklist {
    size_t num;
    hook* hptr;
} *hooklist;


int hook_add(hooklist h)
{
}

hooklist hook_init()
{
    hooklist hooks;
    hooks = SCALLOC(1,sizeof(*hooks));
    return hooks;
}

void hook_free(hooklist h)
{
    if(!h)
        return;

    if(h->hptr)
        free(h->hptr);

    free(h);
    return;
}
