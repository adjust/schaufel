#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <utils/config.h>
#include <utils/scalloc.h>
#include "hooks.h"
#include "hooks/copy.h"
#include "hooks/dummy.h"
#include "hooks/xmark.h"
#include "hooks/jsonexport.h"

typedef struct hooklist {
    int64_t num;
    Hptr *hptrarray;
} *Hooklist;

Hptr *hooks_available;

static Hptr _find_hook(const char *name)
{
    Hptr res = NULL;
    Hptr ha = *hooks_available;
    while(ha->name)
    {
        if(strcmp(name,ha->name) == 0) {
            res = SCALLOC(1,sizeof(struct hptr));
            memcpy(res,ha,sizeof(struct hptr));
            break;
        }
        ha++;
    }
    return res;
}

void hooks_register()
{
    /* todo: validators
     *       dynamic module adding
     */
    struct hptr copy = {"copy",&h_copy,&h_copy_init,&h_copy_free,NULL};
    struct hptr dummy = {"dummy",&h_dummy,&h_dummy_init,&h_dummy_free,NULL};
    struct hptr xmark =
        {"xmark",&h_xmark,&h_xmark_init,&h_xmark_free,NULL};
    struct hptr jsonexport =
        {"jsonexport",&h_jsonexport,&h_jsonexport_init,&h_jsonexport_free,NULL};

    hooks_available = SCALLOC(1,sizeof(Hptr));
    *hooks_available = SCALLOC(5,sizeof(struct hptr)); // null terminator

    memcpy(*hooks_available,(void *) &copy,
        sizeof(struct hptr));
    memcpy((*hooks_available)+1,(void *) &dummy,
        sizeof(struct hptr));
    memcpy((*hooks_available)+2,(void *) &xmark,
        sizeof(struct hptr));
    memcpy((*hooks_available)+3,(void *) &jsonexport,
        sizeof(struct hptr));

    return;
}

void hooks_deregister(void)
{
    free(*hooks_available);
    free(hooks_available);
    return;
}

inline bool hooklist_run(Hooklist h, Message msg)
{
    for (int64_t i = 0; i < h->num; i++)
    {
        Hptr hook = *(h->hptrarray+i);

        hook->hook(hook->ctx,msg);
    }
    return true;
}

int hooks_add(Hooklist h, config_setting_t *conf)
{
    size_t list;
    config_setting_t *hook = NULL, *type = NULL;
    Hptr hookptr;

    if(!config_setting_is_list(conf))
        return 0;

    list = config_setting_length(conf);

    for (size_t i = 0; i < list; ++i)
    {
        hook = config_setting_get_elem(conf, i);
        if(hook == NULL)
            abort();

        if(!(type = CONF_GET_MEM(hook, "type", "hooks need a type!")))
            abort();

        hookptr = _find_hook(config_setting_get_string(type));

        if (hookptr == NULL)
            abort(); //TODO: error message

        // this code is ugly
        h->num++;
        h->hptrarray = realloc(h->hptrarray,(h->num*(sizeof(hookptr))));

        if (h->hptrarray == NULL)
            abort();

        hookptr->ctx = hookptr->init(hook);
        *(h->hptrarray+(h->num)-1) = hookptr;
    }
    return 1;
}

Hooklist hook_init()
{
    Hooklist hooks;
    hooks = SCALLOC(1,sizeof(*hooks));
    return hooks;
}

void hook_free(Hooklist h)
{
    Hptr *hooks = h->hptrarray;
    Hptr hook;
    if(!h)
        return;

    if(hooks)
    {
        for (int64_t i = 0; i < h->num; i++)
        {
            hook = *(hooks+i);
            hook->free(hook->ctx);
            free(hook);
        }
    }
    free(hooks);

    free(h);
    return;
}
