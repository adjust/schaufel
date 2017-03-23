#include <consumer.h>

Consumer
consumer_init(char kind, void *opt)
{
    Consumer c;
    switch (kind)
    {
        case 'd':
            c = dummy_consumer_init();
            break;
        case 'f':
            c = file_consumer_init((char *)opt);
            break;
        case 'r':
            c = redis_consumer_init("127.0.0.1", 6379);
            break;
        case 'k':
            c = kafka_consumer_init("127.0.0.1:9092");
            break;
        default:
            return NULL;
    }
    return c;
}

void
consumer_free(Consumer *c)
{
    if ((*c) == NULL)
        return;
    (*c)->consumer_free(c);
}

void
consumer_consume(Consumer c, Message msg)
{
    if (c == NULL)
        return;
    c->consume(c, msg);
}
