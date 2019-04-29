#ifndef _SCHAUFEL_FILE_H_
#define _SCHAUFEL_FILE_H_

#include "consumer.h"
#include "producer.h"
#include "queue.h"
#include "validator.h"


Producer file_producer_init(config_setting_t *config);

void file_producer_free(Producer *p);

void file_producer_produce(Producer p, Message msg);

Consumer file_consumer_init(config_setting_t *config);

void file_consumer_free(Consumer *c);

int file_consumer_consume(Consumer c, Message msg);

Validator file_validator_init();

#endif
