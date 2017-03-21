#ifndef _SCHAUFEL_FILE_H_
#define _SCHAUFEL_FILE_H_

#include <consumer.h>
#include <producer.h>
#include <queue.h>
#include <stdlib.h>
#include <utils/logger.h>

typedef struct Producer *Producer;

Producer file_producer_init(char *fname);

void file_producer_free(Producer *p);

void file_producer_produce(Producer p, Message msg);

typedef struct Consumer *Consumer;

Consumer file_consumer_init(char *fname);

void file_consumer_free(Consumer *c);

void file_consumer_consume(Consumer c, Message msg);

#endif
