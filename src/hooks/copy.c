#include "utils/config.h"
#include "utils/scalloc.h"
#include "hooks/copy.h"

bool h_copy(Context ctx, Message msg)
{
    return true;
}
Context h_copy_init(config_setting_t *config)
{
    Context ctx = SCALLOC(1,sizeof(*ctx));
    return ctx;
}

void h_copy_free(Context ctx)
{
    free(ctx);
    return;
}
