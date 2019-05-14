#include <string.h>
#include <dlfcn.h>

#include "modules.h"
#include "utils/logger.h"
#include "utils/scalloc.h"

/* TODO: temporary */
#include "dummy.h"
#include "exports.h"
#include "file.h"
#include "kafka.h"
#include "redis.h"


/* modules list struct */
typedef struct ModuleNode ModuleNode;

struct ModuleNode
{
    const char     *name;
    ModuleHandler  *handler;
    ModuleNode     *next;
};

/* modules list */
ModuleNode *modules_head = NULL;


extern void
register_module(const char *name, ModuleHandler *handler)
{
    ModuleNode *node = SCALLOC(1, sizeof(ModuleNode));

    /* TODO: check that module with the same name already exists */
    node->name = name;
    node->handler = handler;
    node->next = modules_head;

    modules_head = node;
}

ModuleHandler *
lookup_module(const char *name)
{
    ModuleNode *node = modules_head;

    while(node)
    {
        if (strcmp(node->name, name) == 0)
            return node->handler;

        node = node->next;
    }

    return NULL;
}

/* TODO: temporary */
void
register_builtin_modules(void)
{
    register_file_module();
    register_dummy_module();
    register_kafka_module();
    register_redis_module();
    register_exports_module();

    /* TODO: load only modules listed in config */
    load_module("./postgres.so");
}

bool
load_module(const char *sopath)
{
    void *handle;
    void (*init_func)(void);

    handle = dlopen(sopath, RTLD_NOW);
    if (!handle)
    {
        logger_log("failed to open object '%s': %s", sopath, dlerror());
        return false;
    }

    /* find the address of module initializer function */
    init_func = (void(*)(void)) dlsym(handle, "schaufel_module_init");
    if (!init_func)
    {
        logger_log("could not find symbol 'schaufel_module_init' in '%s': %s",
                   sopath, dlerror());
        return false;
    }

    /* invoke initializer */
    init_func();

    return true;
}
