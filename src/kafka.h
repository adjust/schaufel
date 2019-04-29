#ifndef _SCHAUFEL_KAFKA_H_
#define _SCHAUFEL_KAFKA_H_

#include "consumer.h"
#include "producer.h"
#include "queue.h"
#include "validator.h"


Producer kafka_producer_init(config_setting_t *config);

void kafka_producer_free(Producer *p);

void kafka_producer_produce(Producer p, Message msg);

Consumer kafka_consumer_init(config_setting_t *config);

void kafka_consumer_free(Consumer *c);

int kafka_consumer_consume(Consumer c, Message msg);

Validator kafka_validator_init();

#endif
