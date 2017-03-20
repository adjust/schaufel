#ifndef _SCHAUFEL_DUMMY_H_
#define _SCHAUFEL_DUMMY_H_

#include <queue.h>
#include <producer.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Producer *Producer;

Producer dummy_producer_init();

void dummy_producer_free(Producer *p);

void dummy_producer_produce(Producer p, Message msg);

void dummy_producer_free(Producer *p);

#endif
