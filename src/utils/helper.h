#ifndef _SCHAUFEL_UTILS_HELPER_H_
#define _SCHAUFEL_UTILS_HELPER_H_

#include <utils/logger.h>
#include <utils/array.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct Options {
    char  input;
    char *in_host;
    char *in_broker;
    int   in_port;
    char *in_file;
    char *in_groupid;
    char *in_topic;
    char  output;
    char *out_host;
    char *out_host_replica;
    char *out_broker;
    int   out_port;
    char *out_file;
    char *out_groupid;
    char *out_topic;
    char *logger;
    Array in_hosts;
    Array out_hosts;
    Array out_hosts_replica;
} Options;

int options_validate(Options o);

size_t number_length(long number);

int parse_connstring(char *conninfo, char **hostname, int *port);

bool get_state(const volatile atomic_bool *state);
bool set_state(volatile atomic_bool *state, bool value);

Array parse_hostinfo_master(char *hostinfo);

Array parse_hostinfo_replica(char *hostinfo);

#define UNUSED __attribute__((__unused__))

#endif
