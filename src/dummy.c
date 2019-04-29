#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dummy.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/scalloc.h"


Producer
dummy_producer_init(UNUSED config_setting_t *config)
{
    Producer dummy = SCALLOC(1, sizeof(*dummy));

    dummy->producer_free = dummy_producer_free;
    dummy->produce       = dummy_producer_produce;
    return dummy;
}

void
dummy_producer_produce(UNUSED Producer p, Message msg)
{
    printf("dummy: %s\n", (char *) message_get_data(msg));
}

void
dummy_producer_free(Producer *p)
{
    free(*p);
    *p = NULL;
}

Consumer
dummy_consumer_init(UNUSED config_setting_t *config)
{
    Consumer dummy = SCALLOC(1, sizeof(*dummy));

    dummy->consumer_free = dummy_consumer_free;
    dummy->consume       = dummy_consumer_consume;
    return dummy;
}

int
dummy_consumer_consume(UNUSED Consumer c, Message msg)
{
    char dummy_string_src[] = "{\"type\":\"dummy\"}";
    char *dummy_string = SCALLOC(sizeof(dummy_string_src), sizeof(*dummy_string));
    memcpy(dummy_string, dummy_string_src, sizeof(dummy_string_src));
    message_set_data(msg, dummy_string);
    return 0;
}

void
dummy_consumer_free(Consumer *c)
{
    free(*c);
    *c = NULL;
}

bool
dummy_validator(UNUSED config_setting_t *config)
{
    return true;
}

Validator
dummy_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = &dummy_validator;
    v->validate_producer = &dummy_validator;
    return v;
}
