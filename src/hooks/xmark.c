#include "hooks/xmark.h"
#include "utils/scalloc.h"
#include "queue.h"

typedef struct internal {
    uint32_t xmark;
} *Internal;


bool h_xmark(Context ctx, Message msg)
{
    message_set_xmark(msg,((Internal) (ctx->data))->xmark);
    return true;
}

Context h_xmark_init(config_setting_t *config)
{
    uint32_t xmark = 0;
    config_setting_t *child = NULL;
    Context ctx = SCALLOC(1,sizeof(*ctx));
    Internal internal = SCALLOC(1,sizeof(*internal));
    ctx->data = (void *) internal;

    if(!(CONF_L_IS_INT(config, "xmark", &xmark, "Hook XMARK requires integer xmark")))
        abort();
    internal->xmark = xmark;

    return ctx;
}

void h_xmark_free(Context ctx)
{
    if (ctx == NULL)
        return;

    free(ctx->data);
    free(ctx);

    return;
}

bool h_xmark_validate(config_setting_t *config)
{
    uint32_t xmark = 0;

    /* todo : xmark is supposed to be unsigned, but input can be signed */
    if(!(CONF_L_IS_INT(config, "xmark", &xmark, "Hook XMARK requires integer xmark")))
        abort();

    return true;
}
