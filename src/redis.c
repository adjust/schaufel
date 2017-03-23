#include <redis.h>

typedef struct Meta {
    redisContext *c;
    redisReply   *reply;
    char         *topic;
} *Meta;

Meta
redis_meta_init(char *hostname, int port, char *topic)
{
    Meta m = calloc(1, sizeof(*m));
    if (m == NULL)
    {
        logger_log("%s %d: allocate failed", __FILE__, __LINE__);
        abort();
    }

    struct timeval timeout = { 1, 500000 };
    m->c = redisConnectWithTimeout(hostname, port, timeout);

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

Producer
redis_producer_init(char *hostname, int port, char *topic)
{
    Producer redis = calloc(1, sizeof(*redis));

    redis->meta          = redis_meta_init(hostname, port, topic);
    redis->producer_free = redis_producer_free;
    redis->produce       = redis_producer_produce;

    return redis;
}

void
redis_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;
    m->reply = redisCommand(m->c, "LPUSH %s %s",m->topic, (char *) message_get_data(msg));
    freeReplyObject(m->reply);
}

void
redis_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);
    redis_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
redis_consumer_init(char *hostname, int port, char *topic)
{
    Consumer redis = calloc(1, sizeof(*redis));

    redis->meta          = redis_meta_init(hostname, port, topic);
    redis->consumer_free = redis_consumer_free;
    redis->consume       = redis_consumer_consume;

    return redis;
}

void
redis_consumer_consume(Consumer c, Message msg)
{
    Meta m = (Meta)c->meta;
    m->reply = redisCommand(m->c, "BLPOP %s", m->topic);
    char *result = calloc(m->reply->len + 1, sizeof(*result));
    strncpy(result, m->reply->str, m->reply->len);
    message_set_data(msg, result);
    freeReplyObject(m->reply);
}

void
redis_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    redis_meta_free(&m);
    free(*c);
    *c = NULL;
}
