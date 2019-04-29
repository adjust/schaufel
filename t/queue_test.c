#include <string.h>

#include "queue.h"
#include "test/test.h"


int
main(void)
{
    Queue q = queue_init();
    Message msg = message_init();
    char *data = "moep";
    queue_add(q, data, strlen(data), 1);
    queue_get(q, msg);
    pretty_assert(strncmp(data, (char *) message_get_data(msg), strlen(data)) == 0);
    queue_free(&q);
    message_free(&msg);
    return 0;
}
