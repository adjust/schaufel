#include <consumer.h>

Consumer
consumer_init(char kind)
{
    Consumer c;
    switch (kind)
    {
        case 'd':
            c = dummy_consumer_init();
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
