#include <dummy.h>

Producer
dummy_producer_init()
{
    Producer dummy = calloc(1, sizeof(*dummy));
    dummy->producer_free = dummy_producer_free;
    dummy->produce       = dummy_producer_produce;
    return dummy;
}

void
dummy_producer_produce(Producer p, Message msg)
{
    printf("dummy: %s\n", (char *) message_get_data(msg));
}

void
dummy_producer_free(Producer *p)
{
    free(*p);
    *p = NULL;
}
