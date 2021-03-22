#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <utils/config.h>
#include <utils/scalloc.h>
#include "hooks.h"
#include "hooks/dummy.h"
#include "hooks/xmark.h"
#include "hooks/jsonexport.h"

typedef struct hooklist {
    uint64_t num;
    Hptr *hptrarray;
} *Hooklist;

Hptr *hooks_available;

static Hptr _find_hook(const char *name)
{
    Hptr res = NULL;

    if(hooks_available == NULL)
        return NULL;

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
    struct hptr dummy =
        {"dummy",&h_dummy,&h_dummy_init,&h_dummy_validate,&h_dummy_free,NULL};
    struct hptr xmark =
        {"xmark",&h_xmark,&h_xmark_init,&h_xmark_validate,&h_xmark_free,NULL};
    struct hptr jsonexport =
        {"jsonexport",&h_jsonexport,&h_jsonexport_init,&h_jsonexport_validate,&h_jsonexport_free,NULL};

    hooks_available = SCALLOC(1,sizeof(Hptr));
    *hooks_available = SCALLOC(4,sizeof(struct hptr)); // null terminator

    memcpy((*hooks_available),(void *) &dummy,
        sizeof(struct hptr));
    memcpy((*hooks_available)+1,(void *) &xmark,
        sizeof(struct hptr));
    memcpy((*hooks_available)+2,(void *) &jsonexport,
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
    bool res;
    for (uint64_t i = 0; i < h->num; i++)
    {
        Hptr hook = *(h->hptrarray+i);

        res = hook->hook(hook->ctx,msg);
        if(!res){
            // free message (unusable)
            free(message_get_data(msg));
            message_set_data(msg, NULL);
            message_set_len(msg,0);
            metadata_free(message_get_metadata(msg));

            return false;
        }
    }
    return true;
}

bool hooks_validate(config_setting_t *conf)
{
    bool res = true;

    size_t list;
    config_setting_t *hook = NULL, *type = NULL;
    Hptr hookptr = NULL;

    if(!config_setting_is_list(conf)) {
        res = false;
        goto error;
    }

    list = config_setting_length(conf);

    for (size_t i = 0; i < list; ++i)
    {
        hook = config_setting_get_elem(conf, i);
        if(hook == NULL) // this error is fatal
            abort();

        if(!(type = CONF_GET_MEM(hook, "type", "hooks need a type!")))
        {
            res = false;
            goto next;
        }

        const char *name = config_setting_get_string(type);
        if(name == NULL)
        {
            fprintf(stderr, "%s %d: hook is not of type string!",
                __FILE__, __LINE__);
            res = false;
            goto next;
        }

        hookptr = _find_hook(name);
        if(hookptr == NULL)
        {
            fprintf(stderr, "%s %d: %s not a valid hook type",
                __FILE__, __LINE__, name);
            res = false;
            goto next;
        }

        res &= hookptr->validate(hook);

        next:
        free(hookptr);
    }

    error:
    return res;
}

void hooks_add(Hooklist h, config_setting_t *conf)
{
    size_t list;
    config_setting_t *hook = NULL, *type = NULL;
    Hptr hookptr;

    list = config_setting_length(conf);

    for (size_t i = 0; i < list; ++i)
    {
        hook = config_setting_get_elem(conf, i);

        if(!(type = CONF_GET_MEM(hook, "type", "hooks need a type!")))
            abort();

        hookptr = _find_hook(config_setting_get_string(type));

        // this code is ugly
        h->num++;
        h->hptrarray = realloc(h->hptrarray,(h->num*(sizeof(hookptr))));

        if (h->hptrarray == NULL)
            abort();

        hookptr->ctx = hookptr->init(hook);
        *(h->hptrarray+(h->num)-1) = hookptr;
    }
    return;
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
        for (uint64_t i = 0; i < h->num; i++)
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
