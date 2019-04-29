#ifndef _SCHAUFEL_UTILS_HELPER_H_
#define _SCHAUFEL_UTILS_HELPER_H_

#include <stdatomic.h>
#include <stdbool.h>

#include "utils/options.h"


int options_validate(Options o);

size_t number_length(long number);

int parse_connstring(const char *conninfo, char **hostname, int *port);

bool get_state(const volatile atomic_bool *state);
bool set_state(volatile atomic_bool *state, bool value);

Array parse_hostinfo_master(char *hostinfo);

Array parse_hostinfo_replica(char *hostinfo);

#define NORETURN __attribute__((__noreturn__))
#define UNUSED __attribute__((__unused__))

#endif
