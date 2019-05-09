#ifndef _SCHAUFEL_UTILS_OPTIONS_H
#define _SCHAUFEL_UTILS_OPTIONS_H

#include "utils/array.h"


typedef struct Options {
    char *config;
    int   consumer_threads;
    int   producer_threads;
    char  input;
    char *in_host;
    char *in_broker;
    int   in_pipeline;
    char *in_file;
    char *in_groupid;
    char *in_topic;
    char  output;
    char *out_host;
    char *out_host_replica;
    char *out_broker;
    int   out_pipeline;
    char *out_file;
    char *out_groupid;
    char *out_topic;
    char *logger;
    Array in_hosts;
    Array out_hosts;
    Array out_hosts_replica;
} Options;

#endif
