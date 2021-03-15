#include "utils/config.h"
#include "utils/scalloc.h"
#include "utils/helper.h"
#include "hooks/dummy.h"

bool h_dummy(UNUSED Context ctx, UNUSED Message msg)
{
    return true;
}

Context h_dummy_init(UNUSED config_setting_t *config)
{
    Context ctx = SCALLOC(1,sizeof(*ctx));
    return ctx;
}

bool h_dummy_validate(UNUSED config_setting_t *config)
{
    return true;
}

void h_dummy_free(Context ctx)
{
    free(ctx);
    return;
}

