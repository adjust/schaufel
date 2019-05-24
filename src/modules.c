#include <string.h>
#include <dlfcn.h>

#include "build.h"
#include "dummy.h"
#include "file.h"
#include "modules.h"
#include "paths.h"
#include "utils/logger.h"
#include "utils/scalloc.h"


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

void
register_builtin_modules(void)
{
    register_file_module();
    register_dummy_module();
}

bool
load_library(const char *name)
{
    void *handle;
    char sopath[2048];
    void (*init_func)(void);

    if (!name)
        return false;

    snprintf(sopath, 2048, "%s/%s.so", CONTRIBDIR, name);
    
    handle = dlopen(sopath, RTLD_NOW);
    if (!handle)
    {
        logger_log("failed to open object '%s': %s", sopath, dlerror());
        return false;
    }

    /*
     * find the address of module initializer function
     *
     * XXX: ISO C standard does not allow to convert a pointer to data (void *) 
     * to a function pointer and vice versa (as they can belong to different
     * address spaces and have different size etc, see Harvard
     * architecture). For instance, gcc with -pedantic throws a warning. The
     * conversion below is a workaround recommended by POSIX and its purpose is
     * to suppress warning message. Strictly speaking it does not follow
     * the strict aliasing rule and theoretically may cause undefined
     * behaviour.
     */
    *(void**)(&init_func) = dlsym(handle, "schaufel_init");
    if (!init_func)
    {
        logger_log("could not find symbol 'schaufel_init' in '%s': %s",
                   sopath, dlerror());
        return false;
    }

    /* invoke initializer */
    init_func();

    return true;
}

bool
load_libraries(config_t *config)
{
    config_setting_t *root;
    config_setting_t *libs;
    int nlibs;
    int i;

    root = config_root_setting(config);
    libs = config_setting_get_member(root, "libraries");
    if (!libs)
    {
        /* no libraries specified */
        return true;
    }

    nlibs = config_setting_length(libs);
    for (i = 0; i < nlibs; ++i)
    {
        config_setting_t *lib = config_setting_get_elem(libs, i);
        const char *libname;

        if (!lib)
            return false;

        libname = config_setting_get_string(lib);
        if (!libname)
            return false;

        if (!load_library(libname))
            return false;
    }

    return true;
}
