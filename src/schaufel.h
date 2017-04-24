#ifndef _SCHAUFEL_H_
#define _SCHAUFEL_H_

#include <utils/logger.h>
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
    char *out_broker;
    int   out_port;
    char *out_file;
    char *out_groupid;
    char *out_topic;
    char *logger;
} Options;

int options_validate(Options o);

size_t number_length(long number);

int parse_connstring(char *conninfo, char **hostname, int *port);

#endif
