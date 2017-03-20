#ifndef _SCHAUFEL_DUMMY_H_
#define _SCHAUFEL_DUMMY_H_

#include <queue.h>
#include <producer.h>
#include <stdlib.h>

typedef struct Producer *Producer;

Producer dummy_init();

void dummy_free(Producer *p);

void dummy_produce(Producer p, Message msg);

void dummy_free(Producer *p);

#endif
