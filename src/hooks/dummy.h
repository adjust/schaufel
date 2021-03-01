#include "hooks.h"
#include "utils/config.h"

int     h_dummy(Context ctx, Message msg);
Context h_dummy_init(config_setting_t *config);
bool    h_dummy_validate(config_setting_t *config);
void    h_dummy_free(Context ctx);
