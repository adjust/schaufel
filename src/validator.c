#include "bagger.h"
#include "dummy.h"
#include "exports.h"
#include "file.h"
#include "kafka.h"
#include "postgres.h"
#include "redis.h"
#include "validator.h"

Validator
validator_init(const char* kind)
{
    Validator v;
    switch (*kind)
    {
        case 'd':
            v = dummy_validator_init();
            break;
        case 'f':
            v = file_validator_init();
            break;
        case 'e':
            v = exports_validator_init();
            break;
        case 'p':
            v = postgres_validator_init();
            break;
        case 'r':
            v = redis_validator_init();
            break;
        case 'k':
            v = kafka_validator_init();
            break;
        case 'b':
            v = bagger_validator_init();
            break;
        default:
            return NULL;
    }

    return v;
}
