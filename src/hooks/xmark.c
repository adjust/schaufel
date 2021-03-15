#include <string.h>
#include "hooks/xmark.h"
#include "utils/scalloc.h"
#include "utils/metadata.h"
#include "queue.h"
#include "utils/fnv.h"
#include "utils/logger.h"

typedef struct internal {
    uint32_t xmark;
    const char *field; // managed by libconfig
    Fnv32_t (*hash) (void *, size_t);
    Fnv32_t (*fold) (Fnv32_t);
} *Internal;


bool h_xmark(Context ctx, Message msg)
{
    Internal i = (Internal) ctx->data;
    Metadata *m = message_get_metadata(msg);

    if(i->field)
    {
        MDatum md = metadata_find(m,(char *)i->field);
        if(!md)
        {
            fprintf(stderr, "Metadata field: \"%s\" does not exist\n", i->field);
            goto fallback;
        }
        if(md->type != MTYPE_STRING)
        {
            fprintf(stderr, "Metadata field: \"%s\" not a string\n", i->field);
            goto fallback;
        }

        uint32_t xmark = i->fold(i->hash(md->value,(md->len-1)));
        message_set_xmark(msg,xmark);
    } else {
        message_set_xmark(msg,i->xmark);
    }

    return true;

    fallback:
    // This message is still okay to produce, but we should warn
    logger_log("%s %d: Falling back to xmark: \"%d\"",
        __FILE__,__LINE__, i->xmark);
    message_set_xmark(msg,i->xmark);
    return true;
}

Context h_xmark_init(config_setting_t *config)
{
    uint32_t xmark = 0;
    config_setting_t *child = NULL;
    const char *res;
    Context ctx = SCALLOC(1,sizeof(*ctx));
    Internal internal = SCALLOC(1,sizeof(*internal));
    ctx->data = (void *) internal;

    child = config_setting_get_member(config, "field");
    if(child) {
        if(!CONF_L_IS_STRING(config,"field", &res, "field must be a string"))
            abort();
        internal->field = res;
        if(!CONF_L_IS_STRING(config,"hash", &res, "hash must be a string"))
            abort();
        internal->hash = fnv_init((char *)res);

        child = config_setting_get_member(config, "fold");
        if(child) {
            if(!CONF_L_IS_STRING(config,"fold", &res, "fold must be a string"))
                abort();
            internal->fold = fold_init((char*)res);
        } else {
            internal->fold = fold_init("fold_noop");
        }
    }

    if(!(CONF_L_IS_INT(config, "xmark", (int32_t *) &xmark,
        "Hook XMARK requires integer xmark (as a fallback)")))
        abort();

    internal->xmark = xmark;

    return ctx;
}

void h_xmark_free(Context ctx)
{
    if(ctx == NULL)
        return;

    free(ctx->data);
    free(ctx);

    return;
}


bool h_xmark_validate(config_setting_t *config)
{
    uint32_t xmark = 0;

    // signedness of xmark should not matter in twos complement archs
    if(!(CONF_L_IS_INT(config, "xmark", (int32_t *) &xmark,
        "Hook XMARK requires integer xmark (as a fallback)")))
        abort();

    return true;
}
