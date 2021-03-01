#include "utils/config.h"
#include "utils/scalloc.h"
#include "utils/helper.h"
#include "hooks/dummy.h"

int h_dummy(UNUSED Context ctx, UNUSED Message msg)
{
    return 0;
}
Context h_dummy_init(UNUSED config_setting_t *config)
{
    Context ctx = SCALLOC(1,sizeof(*ctx));
    return ctx;
}

void h_dummy_free(Context ctx)
{
    free(ctx);
    return;
}
