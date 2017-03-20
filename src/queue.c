#include <queue.h>

typedef struct Message
{
    void    *data;
    int64_t  msgtype;
} *Message;

Message
message_init(void)
{
    Message message = calloc(1, sizeof(*message));
    return message;
}

void *
message_get_data(Message message)
{
    if (message == NULL)
        return NULL;
    return message->data;
}

void
message_set_data(Message message, void *data)
{
    message->data = data;
}

void
message_free(Message *message)
{
    free(*message);
    *message = NULL;
}

typedef struct MessageList *MessageList;

typedef struct MessageList
{
    Message msg;
    MessageList next;
} *MessageList;

MessageList
message_list_init(void)
{
    MessageList msglist = calloc(1, sizeof(*msglist));
    msglist->msg = message_init();
    return msglist;
}

void
message_list_free(MessageList *msglist)
{
    message_free(&((*msglist)->msg));
    free(*msglist);
    *msglist = NULL;
}

typedef struct Queue
{
    int64_t length;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    MessageList first;
    MessageList last;
} *Queue;

Queue
queue_init(void)
{
    Queue queue = calloc(1, sizeof(*queue));

    if (pthread_cond_init(&queue->cond, NULL) != 0)
        return NULL;

    if (pthread_mutex_init(&queue->mutex, NULL) != 0)
    {
        pthread_cond_destroy(&queue->cond);
        return NULL;
    }

    return queue;
}

int
queue_add(Queue queue, void *data, int64_t msgtype)
{
    MessageList newmsg;
    pthread_mutex_lock(&queue->mutex);
    newmsg = message_list_init();
    if (newmsg == NULL)
    {
        pthread_mutex_unlock(&queue->mutex);
        return ENOMEM;
    }

    newmsg->msg->data = data;
    newmsg->msg->msgtype = msgtype;

    newmsg->next = NULL;
    if (queue->last == NULL)
    {
        queue->last = newmsg;
        queue->first = newmsg;
    }
    else
    {
        queue->last->next = newmsg;
        queue->last = newmsg;
    }

    if(queue->length == 0)
        pthread_cond_broadcast(&queue->cond);

    queue->length++;
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int
queue_get(Queue queue, Message msg)
{
    MessageList firstrec;

    if (queue == NULL || msg == NULL)
        return EINVAL;

    pthread_mutex_lock(&queue->mutex);

    while (queue->first == NULL)
        pthread_cond_wait(&queue->cond, &queue->mutex);

    firstrec = queue->first;
    queue->first = queue->first->next;
    queue->length--;

    if (queue->first == NULL)
    {
        queue->last = NULL;
        queue->length = 0;
    }

    msg->data = firstrec->msg->data;
    msg->msgtype = firstrec->msg->msgtype;

    message_list_free(&firstrec);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int
queue_free(Queue *queue)
{
    MessageList rec;
    MessageList next;
    int ret;
    if (*queue == NULL)
    {
        return EINVAL;
    }

    pthread_mutex_lock(&(*queue)->mutex);
    rec =  (*queue)->first;
    while (rec)
    {
        next = rec->next;
        message_free(&(rec->msg));
        message_list_free(&rec);
        rec = next;
    }

    pthread_mutex_unlock(&(*queue)->mutex);
    ret = pthread_mutex_destroy(&(*queue)->mutex);
    pthread_cond_destroy(&(*queue)->cond);
    free(*queue);
    *queue = NULL;
    return ret;
}

long
queue_length(Queue queue)
{
    int64_t counter;
    pthread_mutex_lock(&queue->mutex);
    counter = queue->length;
    pthread_mutex_unlock(&queue->mutex);
    return counter;
}
