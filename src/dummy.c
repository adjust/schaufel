#include <dummy.h>

Producer
dummy_init()
{
    Producer dummy = calloc(1, sizeof(*dummy));
    dummy->producer_free = dummy_free;
    dummy->produce       = dummy_produce;
    return dummy;
}

void
dummy_produce(Producer p, Message msg)
{
    printf("dummy: %s\n", (char *) message_get_data(msg));
}

void
dummy_free(Producer *p)
{
    free(*p);
    *p = NULL;
}
