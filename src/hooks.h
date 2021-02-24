#ifndef _SCHAUFEL_HOOKS_H_
#define _SCHAUFEL_HOOKS_H_

#include <stdlib.h>
#include <stdint.h>
#include "queue.h"

typedef int (*hook)(Message msg);
typedef struct hooklist *hooklist;

int hook_add(hooklist hooks);
hooklist hook_init();
void hook_free(hooklist hooks);

#endif
