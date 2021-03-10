#ifndef _SCHAUFEL_HOOK_EXPORTS_H_
#define _SCHAUFEL_HOOK_EXPORTS_H_

#include <libconfig.h>

#include "queue.h"
#include "hooks.h"


bool    h_jsonexport(Context ctx, Message msg);
Context h_jsonexport_init(config_setting_t *config);
void    h_jsonexport_free(Context ctx);
bool    h_jsonexport_validate(config_setting_t *config);

#endif
