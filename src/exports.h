#ifndef _SCHAUFEL_EXPORTS_H_
#define _SCHAUFEL_EXPORTS_H_

#include <libconfig.h>

#include "producer.h"
#include "consumer.h"
#include "validator.h"
#include "queue.h"


Producer exports_producer_init(config_setting_t *config);

void exports_producer_free(Producer *p);

void exports_producer_produce(Producer p, Message msg);

Consumer exports_consumer_init(config_setting_t *config);

void exports_consumer_free(Consumer *c);

int exports_consumer_consume(Consumer c, Message msg);

Validator exports_validator_init();

#endif
