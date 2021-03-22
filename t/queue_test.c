#include <string.h>

#include "utils/config.h"
#include "queue.h"
#include "test/test.h"


int
main(void)
{
    config_t conf_root;
    config_init(&conf_root);
    config_setting_t *config = config_root_setting(&conf_root);
    config_setting_add(config,"postadd",CONFIG_TYPE_LIST);
    config_setting_add(config,"preget",CONFIG_TYPE_LIST);
    Queue q = queue_init(config);
    Message msg = message_init();
    Metadata *md = message_get_metadata(msg);
    message_set_xmark(msg,1);
    char *data = "moep";
    queue_add(q, data, strlen(data), 1, md);
    queue_get(q, msg);
    pretty_assert(strncmp(data, (char *) message_get_data(msg), strlen(data)) == 0);
    queue_free(&q);
    message_free(&msg);
    config_destroy(&conf_root);
    return 0;
}
