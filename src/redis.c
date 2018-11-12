#include <redis.h>

typedef struct Meta {
    redisContext *c;
    redisReply   *reply;
    char         *topic;
    size_t        pipe_cur;
    size_t        pipe_max;
    bool          pipe_full;
} *Meta;

Meta
redis_meta_init(char *host, char *topic, size_t pipe_max)
{
    Meta m = calloc(1, sizeof(*m));
    if (m == NULL)
    {
        logger_log("%s %d: allocate failed", __FILE__, __LINE__);
        abort();
    }

    char *hostname = NULL;
    int   port = 0;

    if (parse_connstring(host, &hostname, &port) == -1)
        abort();

    struct timeval timeout = { 1, 500000 };
    m->c = redisConnectWithTimeout(hostname, port, timeout);
    m->pipe_max = pipe_max;

    if (m->c == NULL || m->c->err)
    {
        if (m->c)
        {
            logger_log("%s %d: redis connection failed:", __FILE__, __LINE__, m->c->errstr);
            redisFree(m->c);
            abort();
        }
        else
        {
            logger_log("%s %d: allocate failed", __FILE__, __LINE__);
        }
    }

    m->topic = topic;
    return m;
}

void
redis_meta_free(Meta *m)
{
    redisFree((*m)->c);
    free(*m);
    *m = NULL;
}

static int
redis_meta_get_reply(Meta m)
{
    int ret;
    redisReply *reply;

    if ((ret = redisGetReply(m->c, (void *)&reply)) == REDIS_OK)
        m->reply = reply;
    else
        logger_log("%s %d: %s %s", __FILE__, __LINE__, m->c->errstr,
                   m->c->err == REDIS_ERR_IO ? strerror(errno) : "");
    return ret;
}

static ssize_t
redis_meta_check_pipeline(Meta m, bool lazy)
{
    if (m->pipe_cur == 0)
       return 0;
    else if (lazy && m->pipe_cur < m->pipe_max)
       return -1;

    ssize_t count = m->pipe_cur;
    do {
        if (redis_meta_get_reply(m) == REDIS_OK)
            freeReplyObject(m->reply);
        m->pipe_cur--;
    } while (m->pipe_cur > 0);

    return count;
}

Producer
redis_producer_init(char *host, char *topic, size_t pipeline)
{
    Producer redis = calloc(1, sizeof(*redis));

    redis->meta          = redis_meta_init(host, topic, pipeline);
    redis->producer_free = redis_producer_free;
    redis->produce       = redis_producer_produce;

    return redis;
}

void
redis_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;

    if (m->pipe_max == 0) { /* No pipelining. */
        m->reply = redisCommand(m->c, "LPUSH %s %b",m->topic, (char *) message_get_data(msg), message_get_len(msg));
        freeReplyObject(m->reply);
    } else { /* Pipelining */
        redisAppendCommand(m->c, "LPUSH %s %b",m->topic, (char *) message_get_data(msg), message_get_len(msg));
        m->pipe_cur++;
        redis_meta_check_pipeline(m, true);
    }
}

void
redis_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);
    redis_meta_check_pipeline(m, false);
    redis_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
redis_consumer_init(char *host, char *topic, size_t pipeline)
{
    Consumer redis = calloc(1, sizeof(*redis));

    redis->meta          = redis_meta_init(host, topic, pipeline);
    redis->consumer_free = redis_consumer_free;
    redis->consume       = redis_consumer_consume;

    return redis;
}

static void
redis_consumer_handle_reply(const redisReply *reply, Message msg)
{
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2)
    {
        char *result = calloc(reply->element[1]->len + 1, sizeof(*result));
        memcpy(result, reply->element[1]->str, reply->element[1]->len);
        message_set_data(msg, result);
        message_set_len(msg, reply->element[1]->len);
    }
}

int
redis_consumer_consume(Consumer c, Message msg)
{
    Meta m = (Meta)c->meta;

    if (m->pipe_max == 0) {
        /* No pipelining. */
        m->reply = redisCommand(m->c, "BLPOP %s 1", m->topic);
        redis_consumer_handle_reply(m->reply, msg);
        freeReplyObject(m->reply);
        return 0;
    }

    /* Pipelining */
    if (m->pipe_cur < m->pipe_max) {
        redisAppendCommand(m->c, "BLPOP %s 1", m->topic);
        m->pipe_cur++;
    }

    if (m->pipe_cur >= m->pipe_max)
        m->pipe_full = true;

    if (m->pipe_full) {
        if (redis_meta_get_reply(m) == REDIS_OK) {
            redis_consumer_handle_reply(m->reply, msg);
            freeReplyObject(m->reply);
        }
        m->pipe_cur--;
        if (m->pipe_cur == 0)
            m->pipe_full = false;
    }

    return 0;
}

void
redis_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    redis_meta_free(&m);
    free(*c);
    *c = NULL;
}
