#include <producer.h>

Producer
producer_init(char kind, void *opt)
{
    Producer p;
    switch (kind)
    {
        case 'd':
            p = dummy_producer_init();
            break;
        case 'f':
            p = file_producer_init((char *)opt);
            break;
        case 'r':
            p = redis_producer_init("127.0.0.1", 6379);
            break;
        case 'k':
            p = kafka_producer_init("127.0.0.1:9092");
            break;
        default:
            return NULL;
    }
    return p;
}

void
producer_free(Producer *p)
{
    if ((*p) == NULL)
        return;
    (*p)->producer_free(p);
}

void
producer_produce(Producer p, Message msg)
{
    if (p == NULL)
        return;
    p->produce(p, msg);
}
