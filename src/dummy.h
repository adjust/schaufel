#ifndef _SCHAUFEL_DUMMY_H_
#define _SCHAUFEL_DUMMY_H_

#include "consumer.h"
#include "producer.h"
#include "queue.h"
#include "validator.h"


Producer dummy_producer_init(config_setting_t *config);

void dummy_producer_free(Producer *p);

void dummy_producer_produce(Producer p, Message msg);

Consumer dummy_consumer_init();

void dummy_consumer_free(Consumer *c);

int dummy_consumer_consume(Consumer c, Message msg);

Validator dummy_validator_init();

#endif
