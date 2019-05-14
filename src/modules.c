#include <string.h>

#include "modules.h"
#include "utils/scalloc.h"

/* TODO: temporary */
#include "dummy.h"
#include "exports.h"
#include "file.h"
#include "kafka.h"
#include "postgres.h"
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
    register_postgres_module();
    register_redis_module();
    register_exports_module();
}
