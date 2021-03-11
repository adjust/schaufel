#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>

#include "utils/config.h"
#include "queue.h"
#include "hooks.h"

typedef struct Message
{
    void    *data;
    size_t   datalen;
    int64_t  xmark;
    Metadata metadata;
} *Message;

Message
message_init(void)
{
    Message msg = calloc(1, sizeof(*msg));
    return msg;
}

Metadata *
message_get_metadata(Message msg)
{
    if (msg == NULL)
        return NULL;
    return &(msg->metadata);
}

void
message_set_metadata(Message msg, Metadata md)
{
    if (msg)
        msg->metadata = md;
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

int64_t
message_get_xmark(Message msg)
{
    if (msg == NULL)
        return 0;
    return msg->xmark;
}

void
message_set_xmark(Message msg, int64_t xmark)
{
    if(msg)
       msg->xmark = xmark;
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
    Hooklist postadd;
    Hooklist preget;
} *Queue;

Queue
queue_init(config_setting_t *conf)
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

    q->postadd = hook_init();
    q->preget = hook_init();

    /* todo: error handling */
    hooks_add(q->postadd,config_setting_get_member(conf, "postadd"));
    hooks_add(q->postadd,config_setting_get_member(conf, "preget"));

    q->timeout.tv_sec = 10;
    q->timeout.tv_nsec = 0;

    return q;
}

int
queue_add(Queue q, void *data, size_t datalen, int64_t xmark, Metadata *md)
{
    MessageList newmsg;
    /* Todo:
     *      Reorder this function (mutexes)
     *      Pass Message msg to this function
     */
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
    newmsg->msg->xmark = xmark;
    newmsg->msg->metadata = *md;

    hooklist_run(q->postadd,newmsg->msg);

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
    MessageList cur, prev = NULL;
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

    /* todo : optimize this part to be done without locks
     * as it is O(n) while holding a mutex
     */

    // find message matching consumers xmark
    cur = q->first;

    do {
        // match xmark of caller
        if (cur->msg->xmark == msg->xmark)
            break;
        prev = cur;
        cur = cur->next;
    } while (prev->next != NULL);

    if (cur == NULL) // This will cause cpu cycles wasted on polling queue
    {
        pthread_mutex_unlock(&q->mutex);
        return ETIMEDOUT;
    }

    if (prev == NULL) {
        // pop from start
        firstrec = q->first;
        q->first = q->first->next;
    } else {
        // splice element out of list
        firstrec = cur;

        if (cur->next) // element is in list
            prev->next = cur->next;
        else           // element is last
        {
            prev->next = NULL;
            q->last = prev;
        }
    }


    q->length--;
    q->delivered++;

    if (q->first == NULL)
    {
        q->last = NULL;
        q->length = 0;
    }

    msg->data = firstrec->msg->data;
    msg->datalen = firstrec->msg->datalen;
    msg->metadata = firstrec->msg->metadata;

    /* this line can cause an unfinishable queue
     * consumers do not need xmark anylonger
     */
    // msg->xmark = firstrec->msg->xmark;

    pthread_cond_broadcast(&q->consumer_cond);
    message_list_free(&firstrec);
    pthread_mutex_unlock(&q->mutex);

    hooklist_run(q->preget,msg);

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

    hook_free((*q)->postadd);
    hook_free((*q)->preget);

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
