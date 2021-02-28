#include "hooks.h"
#include "utils/config.h"

int     h_copy(Context ctx, Message msg);
Context h_copy_init(config_setting_t *config);
bool    h_copy_validate(config_setting_t *config);
void    h_copy_free(Context ctx);
