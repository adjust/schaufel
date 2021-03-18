#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>

#include "utils/config.h"
#include "utils/bintree.h"
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
    MessageList prev;
    MessageList xnext;
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

typedef struct Xmark
{
    MessageList first;
    MessageList last;
    pthread_cond_t producer_cond;
    pthread_cond_t consumer_cond;
    int64_t count;
} *Xmark;

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
    void *xtree;
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

static void _xmark_free(Node n)
{
    Xmark x = (Xmark) n->data;
    pthread_cond_destroy(&x->consumer_cond);
    pthread_cond_destroy(&x->producer_cond);
    free(n->data);
    free(n);
}


static inline Xmark
_xmark_find(Queue q, uint32_t mark)
{
    Xmark x;
    struct node n;
    Node res;
    n.elem = mark;
    res = bin_find((void **) &q->xtree,&n,&bin_intcomp);

    if(!res)
    {
        // allocate new element
        if((res = calloc(1,sizeof(*res))) == NULL)
            goto error;
        res->elem = mark;
        if((x = calloc(1,sizeof(*x))) == NULL)
            goto error;

        if((pthread_cond_init(&x->consumer_cond, NULL)) != 0)
            goto error;
        if((pthread_cond_init(&x->producer_cond, NULL)) != 0)
        {
            pthread_cond_destroy(&x->consumer_cond);
            goto error;
        }
        res->data = (void *) x;
        res->free = &_xmark_free;

        // insert into tree
        res = bin_search((void **) &q->xtree,res,&bin_intcomp);
        if(res == NULL) {
            pthread_cond_destroy(&x->consumer_cond);
            pthread_cond_destroy(&x->producer_cond);
            goto error;
        }
    } else
        x = (Xmark) res->data;

    return x;

    error:  // ENOMEM, return NULL
    if(res)
        free(res->data);
    free(res);
    return NULL;
}

int
queue_add(Queue q, void *data, size_t datalen, int64_t xmark, Metadata *md)
{
    MessageList newmsg;
    /* We can afford to allocate the message before
     * checking queue length */

    newmsg = message_list_init();
    if (newmsg == NULL)
        return ENOMEM;
    newmsg->msg->datalen = datalen;
    newmsg->msg->data = data;
    newmsg->msg->xmark = xmark;
    newmsg->msg->metadata = *md;
    newmsg->next = NULL;
    newmsg->prev = NULL;
    newmsg->xnext = NULL;

    if(!hooklist_run(q->postadd,newmsg->msg))
        return EBADMSG;

    pthread_mutex_lock(&q->mutex);

    Xmark x = _xmark_find(q,xmark);
    if(x == NULL)
    {
        pthread_mutex_unlock(&q->mutex);
        return ENOMEM;
    }

    while (q->length > MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&q->consumer_cond, &q->mutex);
    }

    // add to xmark queue
    if (x->last == NULL)
    {
        x->last = newmsg;
        x->first = newmsg;
    }
    else
    {
        x->last->xnext = newmsg;
        x->last = newmsg;
    }

    // add to global queue
    if (q->last == NULL)
    {
        q->last = newmsg;
        q->first = newmsg;
    }
    else
    {
        newmsg->prev = q->last;
        q->last->next = newmsg;
        q->last = newmsg;
    }

    // unblock waiting threads
    if(q->length == 0)
        pthread_cond_broadcast(&q->producer_cond);
    if(x->count == 0)
        pthread_cond_broadcast(&x->producer_cond);

    x->count++;
    q->length++;
    q->added++;
    pthread_mutex_unlock(&q->mutex);

    return 0;
}

int
queue_get(Queue q, Message msg)
{
    MessageList firstrec;
    MessageList next = NULL, prev = NULL;

    int ret = 0;
    if (q == NULL || msg == NULL)
        return EINVAL;

    pthread_mutex_lock(&q->mutex);
    Xmark x = _xmark_find(q,msg->xmark);

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

    abstimeout.tv_sec  = now.tv_sec + q->timeout.tv_sec;
    abstimeout.tv_nsec = (now.tv_usec*1000) + q->timeout.tv_nsec;
    if (abstimeout.tv_nsec >= 1000000000)
    {
        abstimeout.tv_sec++;
        abstimeout.tv_nsec -= 1000000000;
    }

    while(x->first == NULL && ret != ETIMEDOUT)
    {
        ret = pthread_cond_timedwait(&x->producer_cond, &q->mutex, &abstimeout);
    }

    if (ret == ETIMEDOUT)
    {
        pthread_mutex_unlock(&q->mutex);
        return ret;
    }

    firstrec = x->first;
    x->first = x->first->xnext; // either a message or NULL

    /*splice element out of global list*/
    next = firstrec->next;
    prev = firstrec->prev;

    if (next)
        next->prev = prev;  // either a message or NULL
    else
        q->last = prev;
    if (prev)
        prev->next = next;  // either a message or NULL
    else
        q->first = next;


    x->count--;
    q->length--;
    q->delivered++;

    if (q->first == NULL)
    {
        q->last = NULL;
        q->length = 0;
    }

    if (x->first == NULL)
    {
        x->last = NULL;
        x->count = 0;
    }

    msg->data = firstrec->msg->data;
    msg->datalen = firstrec->msg->datalen;
    msg->metadata = firstrec->msg->metadata;

    /* this line can cause an unfinishable queue
     * consumers do not need xmark anylonger
     */
    // msg->xmark = firstrec->msg->xmark;

    pthread_cond_broadcast(&q->consumer_cond);
    pthread_cond_broadcast(&x->consumer_cond);
    message_list_free(&firstrec);
    pthread_mutex_unlock(&q->mutex);

    if(!hooklist_run(q->preget,msg))
        return EBADMSG;

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

    bin_destroy((*q)->xtree);
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
