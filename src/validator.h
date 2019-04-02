#ifndef _SCHAUFEL_VALIDATOR_H
#define _SCHAUFEL_VALIDATOR_H

#include <dummy.h>
#include <file.h>
#include <kafka.h>
#include <postgres.h>
#include <queue.h>
#include <redis.h>
#include <utils/helper.h>

typedef struct Validator *Validator;

typedef struct Validator {
    int (*validate_consumer) (config_setting_t* config);
    int (*validate_producer) (config_setting_t* config);
} *Validator;

Validator validator_init(char* kind);
#endif
