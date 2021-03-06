#ifndef _SCHAUFEL_QUEUE_H_
#define _SCHAUFEL_QUEUE_H_

#include <stdlib.h>
#include <stdint.h>
#include "utils/config.h"
#include "utils/metadata.h"

#define MAX_QUEUE_SIZE 100000

typedef struct Message *Message;

Message message_init();
Metadata *message_get_metadata(Message msg);
void      message_set_metadata(Message msg, Metadata md);
void     *message_get_data(Message msg);
void      message_set_data(Message msg, void *data);
size_t    message_get_len(Message msg);
int64_t   message_get_xmark(Message msg);
void      message_set_xmark(Message msg, int64_t xmark);
void      message_set_len(Message msg, size_t len);
void      message_free(Message *msg);

typedef struct Queue *Queue;

Queue queue_init(config_setting_t *config);
int  queue_add(Queue q, void *data, size_t datalen, int64_t msgtype, Metadata *md);
int  queue_get(Queue q, Message msg);
long queue_length(Queue q);
long queue_added(Queue q);
long queue_delivered(Queue q);
int  queue_free(Queue *q);

bool queue_validate(config_setting_t *config);

#endif
