#include <queue.h>
#include <consumer.h>
#include <stdio.h>
#include <test/test.h>
#include <utils/logger.h>
#include <schaufel.h>

int
main(void)
{
    Options o;
    memset(&o, '\0', sizeof(o));
    o.in_file = "sample/dummy_file";
    logger_init("log/test");
    Message msg = message_init();
    Consumer c = consumer_init('f', &o);
    char *string;

    consumer_consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "first", 5) == 0);
    free(string);
    consumer_consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "second", 6) == 0);
    free(string);
    consumer_consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "third", 5) == 0);
    free(string);

    message_free(&msg);
    consumer_free(&c);
    logger_free();
    return 0;
}
