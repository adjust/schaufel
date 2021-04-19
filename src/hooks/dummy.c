// This is an example hook providing no functionality
#include "utils/config.h"
#include "utils/scalloc.h"
#include "utils/helper.h"
// This header contains the public api used to initialise this hook
#include "hooks/dummy.h"


// hook function used to transform a message
bool h_dummy(UNUSED Context ctx, UNUSED Message msg)
{
    return true;
}

// memory context for this hook
Context h_dummy_init(UNUSED config_setting_t *config)
{
    Context ctx = SCALLOC(1,sizeof(*ctx));
    return ctx;
}

// Configuration validator (called while parsing the config)
bool h_dummy_validate(UNUSED config_setting_t *config)
{
    return true;
}

// memory context free function
void h_dummy_free(Context ctx)
{
    free(ctx);
    return;
}

