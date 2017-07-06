#include <dummy.h>

Producer
dummy_producer_init()
{
    Producer dummy = calloc(1, sizeof(*dummy));
    if (!dummy) {
        logger_log("%s %d calloc failed\n", __FILE__, __LINE__);
        abort();
    }
    dummy->producer_free = dummy_producer_free;
    dummy->produce       = dummy_producer_produce;
    return dummy;
}

void
dummy_producer_produce(UNUSED Producer p, Message msg)
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
    if (!dummy) {
        logger_log("%s %d calloc failed\n", __FILE__, __LINE__);
        abort();
    }
    dummy->consumer_free = dummy_consumer_free;
    dummy->consume       = dummy_consumer_consume;
    return dummy;
}

int
dummy_consumer_consume(UNUSED Consumer c, Message msg)
{
    char *dummy_string = calloc(17, sizeof(*dummy_string));
    snprintf(dummy_string, 17, "{\"type\":\"dummy\"}");
    message_set_data(msg, dummy_string);
    return 0;
}

void
dummy_consumer_free(Consumer *c)
{
    free(*c);
    *c = NULL;
}
