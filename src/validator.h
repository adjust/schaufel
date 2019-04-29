#ifndef _SCHAUFEL_VALIDATOR_H
#define _SCHAUFEL_VALIDATOR_H

#include <libconfig.h>
#include <stdbool.h>

typedef struct Validator *Validator;

struct Validator {
    bool (*validate_consumer) (config_setting_t* config);
    bool (*validate_producer) (config_setting_t* config);
};

Validator validator_init(const char* kind);
#endif
