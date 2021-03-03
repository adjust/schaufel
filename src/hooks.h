#ifndef _SCHAUFEL_HOOKS_H_
#define _SCHAUFEL_HOOKS_H_

#include <stdlib.h>
#include <stdint.h>
#include "queue.h"
#include "utils/config.h"

typedef struct context {
    config_setting_t *conf;
    void *data;
} *Context;

typedef struct hptr {
    char *name;
    bool  (*hook) (Context ctx, Message msg);
    Context  (*init) (config_setting_t *config);
    void (*free) (Context ctx);
    Context ctx;
} *Hptr;

typedef struct hooklist *Hooklist;
Hooklist hook_init();

int hooks_add(Hooklist hooks, config_setting_t *conf);
void hook_free(Hooklist hooks);
void hooks_register();
void hooks_deregister();
bool hooklist_run();

#endif
