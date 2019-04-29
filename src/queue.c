#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include "queue.h"


typedef struct Message
{
    void    *data;
    size_t   datalen;
    int64_t  msgtype;
} *Message;

Message
message_init(void)
{
    Message msg = calloc(1, sizeof(*msg));
    return msg;
}

void *
message_get_data(Message msg)
{
    if (msg == NULL)
        return NULL;
    return msg->data;
}

void
message_set_data(Message msg, void *data)
{
    if (msg)
        msg->data = data;
}

void
message_set_len(Message msg, size_t len)
{
    if(msg)
       msg->datalen = len;
}

size_t
message_get_len(Message msg)
{
    if (msg == NULL)
        return 0;
    return msg->datalen;
}

void
message_free(Message *msg)
{
    free(*msg);
    *msg = NULL;
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
    if (!msglist)
        return NULL;
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
    struct timespec timeout;
    int64_t length;
    int64_t added;
    int64_t delivered;
    pthread_mutex_t mutex;
    pthread_cond_t producer_cond;
    pthread_cond_t consumer_cond;
    MessageList first;
    MessageList last;
} *Queue;

Queue
queue_init(void)
{
    Queue q = calloc(1, sizeof(*q));
    if (!q)
        return NULL;

    if (pthread_cond_init(&q->producer_cond, NULL) != 0)
        return NULL;

    if (pthread_cond_init(&q->consumer_cond, NULL) != 0)
    {
        pthread_cond_destroy(&q->producer_cond);
        return NULL;
    }

    if (pthread_mutex_init(&q->mutex, NULL) != 0)
    {
        pthread_cond_destroy(&q->producer_cond);
        pthread_cond_destroy(&q->consumer_cond);
        return NULL;
    }

    q->timeout.tv_sec = 10;
    q->timeout.tv_nsec = 0;

    return q;
}

int
queue_add(Queue q, void *data, size_t datalen, int64_t msgtype)
{
    MessageList newmsg;
    pthread_mutex_lock(&q->mutex);

    while (q->length > MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&q->consumer_cond, &q->mutex);
    }

    newmsg = message_list_init();
    if (newmsg == NULL)
    {
        pthread_mutex_unlock(&q->mutex);
        return ENOMEM;
    }

    if (datalen != 0)
    {
        newmsg->msg->datalen = datalen;
    }
    newmsg->msg->data = data;
    newmsg->msg->msgtype = msgtype;

    newmsg->next = NULL;
    if (q->last == NULL)
    {
        q->last = newmsg;
        q->first = newmsg;
    }
    else
    {
        q->last->next = newmsg;
        q->last = newmsg;
    }

    if(q->length == 0)
        pthread_cond_broadcast(&q->producer_cond);

    q->length++;
    q->added++;
    pthread_mutex_unlock(&q->mutex);

    return 0;
}

int
queue_get(Queue q, Message msg)
{
    MessageList firstrec;
    int ret = 0;
    if (q == NULL || msg == NULL)
        return EINVAL;

    pthread_mutex_lock(&q->mutex);

    struct timeval now;
    gettimeofday(&now, NULL);

    struct timespec abstimeout;
    abstimeout.tv_sec  = now.tv_sec + q->timeout.tv_sec;
    abstimeout.tv_nsec = (now.tv_usec*1000) + q->timeout.tv_nsec;
    if (abstimeout.tv_nsec >= 1000000000)
    {
        abstimeout.tv_sec++;
        abstimeout.tv_nsec -= 1000000000;
    }

    while (q->first == NULL && ret != ETIMEDOUT)
    {
        ret = pthread_cond_timedwait(&q->producer_cond, &q->mutex, &abstimeout);
    }

    if (ret == ETIMEDOUT)
    {
        pthread_mutex_unlock(&q->mutex);
        return ret;
    }

    firstrec = q->first;
    q->first = q->first->next;
    q->length--;
    q->delivered++;

    if (q->first == NULL)
    {
        q->last = NULL;
        q->length = 0;
    }

    msg->data = firstrec->msg->data;
    msg->datalen = firstrec->msg->datalen;
    msg->msgtype = firstrec->msg->msgtype;

    pthread_cond_broadcast(&q->consumer_cond);
    message_list_free(&firstrec);
    pthread_mutex_unlock(&q->mutex);

    return 0;
}

int
queue_free(Queue *q)
{
    MessageList rec;
    MessageList next;
    int ret;
    if (*q == NULL)
    {
        return EINVAL;
    }

    pthread_mutex_lock(&(*q)->mutex);
    rec =  (*q)->first;
    while (rec)
    {
        next = rec->next;
        message_free(&(rec->msg));
        message_list_free(&rec);
        rec = next;
    }

    pthread_mutex_unlock(&(*q)->mutex);
    ret = pthread_mutex_destroy(&(*q)->mutex);
    pthread_cond_destroy(&(*q)->producer_cond);
    pthread_cond_destroy(&(*q)->consumer_cond);
    free(*q);
    *q = NULL;
    return ret;
}

long
queue_length(Queue q)
{
    int64_t counter;
    pthread_mutex_lock(&q->mutex);
    counter = q->length;
    pthread_mutex_unlock(&q->mutex);
    return counter;
}

long
queue_added(Queue q)
{
    int64_t added;
    pthread_mutex_lock(&q->mutex);
    added = q->added;
    q->added = 0;
    pthread_mutex_unlock(&q->mutex);
    return added;
}

long
queue_delivered(Queue q)
{
    int64_t delivered;
    pthread_mutex_lock(&q->mutex);
    delivered = q->delivered;
    q->delivered = 0;
    pthread_mutex_unlock(&q->mutex);
    return delivered;
}
