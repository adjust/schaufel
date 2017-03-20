#include <producer.h>

Producer
producer_init(char kind)
{
    Producer p;
    switch(kind)
    {
        case 'd':
            p = dummy_init();
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
    p->produce(p, msg);
}
