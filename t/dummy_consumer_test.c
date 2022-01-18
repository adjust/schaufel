#include "schaufel.h"
#include <stdio.h>
#include <string.h>

#include "utils/config.h"
#include "consumer.h"
#include "queue.h"
#include "test/test.h"


int
main(void)
{
    Message msg = message_init();
    config_t conf_root;
    config_init(&conf_root);
    config_setting_t *config = config_root_setting(&conf_root);
    Consumer c = consumer_init('d', config);
    consumer_consume(c, msg);
    char *string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL) {
        pretty_assert(strncmp(string, "{\"type\":\"dummy\"}", 16) == 0);
        printf("%s\n", string);
    } else
        goto error;
    message_free(&msg);
    free(string);
    consumer_free(&c);
    config_destroy(&conf_root);

    return 0;

    error:
    message_free(&msg);
    free(string);
    consumer_free(&c);
    config_destroy(&conf_root);

    return 1;
}
