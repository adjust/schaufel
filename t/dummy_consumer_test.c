#include <stdio.h>
#include <test/test.h>
#include <string.h>

#include "queue.h"
#include "consumer.h"


int
main(void)
{
    Message msg = message_init();
    Consumer c = consumer_init('d', NULL);
    consumer_consume(c, msg);
    char *string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "{\"type\":\"dummy\"}", 16) == 0);
        printf("%s\n", string);
    message_free(&msg);
    free(string);
    consumer_free(&c);
    return 0;
}
