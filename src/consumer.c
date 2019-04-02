#include <consumer.h>

Consumer
consumer_init(char kind, config_setting_t *config)
{
    Consumer c;
    switch (kind)
    {
        case 'd':
            c = dummy_consumer_init(config);
            break;
        case 'f':
            c = file_consumer_init(config);
            break;
        case 'r':
            c = redis_consumer_init(config);
            break;
        case 'k':
            c = kafka_consumer_init(config);
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

int
consumer_consume(Consumer c, Message msg)
{
    if (c == NULL)
        return -1;
    return c->consume(c, msg);
}
