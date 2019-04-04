#include <producer.h>

Producer
producer_init(char kind, config_setting_t *config)
{
    Producer p;
    switch (kind)
    {
        case 'd':
            p = dummy_producer_init(config);
            break;
        case 'f':
            p = file_producer_init(config);
            break;
        case 'r':
            p = redis_producer_init(config);
            break;
        case 'e':
            p = exports_producer_init(config);
            break;
        case 'p':
            p = postgres_producer_init(config);
            break;
        case 'k':
            p = kafka_producer_init(config);
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
