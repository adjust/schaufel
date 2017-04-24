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
            p = file_producer_init(((Options *)opt)->out_file);
            break;
        case 'r':
            p = redis_producer_init(((Options *)opt)->out_host, ((Options *)opt)->out_port, ((Options *)opt)->out_topic);
            break;
        case 'p':
            p = postgres_producer_init(((Options *)opt)->out_host);
            break;
        case 'k':
            p = kafka_producer_init(((Options *)opt)->out_broker, ((Options *)opt)->out_topic);
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
