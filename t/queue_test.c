#include <test/test.h>
#include <queue.h>

int
main(void)
{
    Queue q = queue_init();
    Message msg = message_init();
    char *data = "moep";
    queue_add(q, data, 1);
    queue_get(q, msg);
    pretty_assert(strncmp(data, (char *) message_data(msg), 4) == 0);
    queue_free(&q);
    message_free(&msg);
    return 0;
}
