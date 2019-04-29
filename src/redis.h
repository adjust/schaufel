#ifndef _SCHAUFEL_REDIS_H_
#define _SCHAUFEL_REDIS_H_

#include "consumer.h"
#include "producer.h"
#include "queue.h"
#include "validator.h"


Producer redis_producer_init(config_setting_t *config);

void redis_producer_free(Producer *p);

void redis_producer_produce(Producer p, Message msg);

Consumer redis_consumer_init(config_setting_t *config);

void redis_consumer_free(Consumer *c);

int redis_consumer_consume(Consumer c, Message msg);

Validator redis_validator_init();
#endif
