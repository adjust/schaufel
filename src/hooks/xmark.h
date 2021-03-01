#include <stdint.h>
#include "hooks.h"

int     xmark(Context ctx, Message msg);
Context xmark_init(config_setting_t *config);
void    xmark_free(Context *ctx);
