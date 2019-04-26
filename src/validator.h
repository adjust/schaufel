#ifndef _SCHAUFEL_VALIDATOR_H
#define _SCHAUFEL_VALIDATOR_H

#include <dummy.h>
#include <file.h>
#include <kafka.h>
#include <postgres.h>
#include <queue.h>
#include <redis.h>
#include <utils/helper.h>
#include <stdbool.h>

typedef struct Validator {
    bool (*validate_consumer) (config_setting_t* config);
    bool (*validate_producer) (config_setting_t* config);
} *Validator;

Validator validator_init(const char* kind);
#endif
