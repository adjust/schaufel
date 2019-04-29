#include <stdio.h>

#include "queue.h"
#include "producer.h"


int
main(void)
{
    Message msg = message_init();
    char *test = malloc(5);
    test[0] = 'm';
    test[1] = 'o';
    test[2] = 'e';
    test[3] = 'p';
    test[4] = '\0';
    message_set_data(msg, test);
    Producer p = producer_init('d', NULL);
    producer_produce(p, msg);
    message_free(&msg);
    producer_free(&p);
    free(test);
    return 0;
}
