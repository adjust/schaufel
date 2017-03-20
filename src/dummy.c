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

Consumer
dummy_consumer_init()
{
    Consumer dummy = calloc(1, sizeof(*dummy));
    dummy->consumer_free = dummy_consumer_free;
    dummy->consume       = dummy_consumer_consume;
    return dummy;
}

void
dummy_consumer_consume(Consumer c, Message msg)
{
    char *dummy_string = calloc(17, sizeof(*dummy_string));
    snprintf(dummy_string, 17, "{\"type\":\"dummy\"}");
    message_set_data(msg, dummy_string);
}

void
dummy_consumer_free(Consumer *c)
{
    free(*c);
    *c = NULL;
}
