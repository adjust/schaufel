#ifndef _SCHAUFEL_HOOK_DUMMY_H_
#define _SCHAUFEL_HOOK_DUMMY_H_

#include "hooks.h"
#include "utils/config.h"

bool h_dummy(Context ctx, Message msg);
Context h_dummy_init(config_setting_t *config);
bool    h_dummy_validate(config_setting_t *config);
void    h_dummy_free(Context ctx);

#endif
