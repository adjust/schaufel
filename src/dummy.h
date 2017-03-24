#ifndef _SCHAUFEL_DUMMY_H_
#define _SCHAUFEL_DUMMY_H_

#include <consumer.h>
#include <queue.h>
#include <producer.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Producer *Producer;

Producer dummy_producer_init();

void dummy_producer_free(Producer *p);

void dummy_producer_produce(Producer p, Message msg);

typedef struct Consumer *Consumer;

Consumer dummy_consumer_init();

void dummy_consumer_free(Consumer *c);

int dummy_consumer_consume(Consumer c, Message msg);

#endif
