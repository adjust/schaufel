#include <validator.h>

Validator
validator_init(char* kind)
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
        case 'p':
            v = postgres_validator_init();
            break;
        case 'r':
            v = redis_validator_init();
            break;
        case 'k':
            v = kafka_validator_init();
            break;
        default:
            return NULL;
    }

    return v;
}
