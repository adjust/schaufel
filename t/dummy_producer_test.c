#include <stdio.h>

#include "utils/config.h"
#include "queue.h"
#include "producer.h"


int
main(void)
{
    Message msg = message_init();
    config_t conf_root;
    config_init(&conf_root);
    config_setting_t *config = config_root_setting(&conf_root);
    char *test = malloc(5);
    test[0] = 'm';
    test[1] = 'o';
    test[2] = 'e';
    test[3] = 'p';
    test[4] = '\0';
    message_set_data(msg, test);
    Producer p = producer_init('d', config);
    producer_produce(p, msg);
    message_free(&msg);
    producer_free(&p);
    config_destroy(&conf_root);
    free(test);
    return 0;
}
