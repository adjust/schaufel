#include "hooks.h"

bool    h_xmark(Context ctx, Message msg);
Context h_xmark_init(config_setting_t *config);
void    h_xmark_free(Context ctx);
bool    h_xmark_validate(config_setting_t *config);
