#include <stdio.h>

#include "dummy.h"
#include "modules.h"
#include "queue.h"


int
main(void)
{
    ModuleHandler *dummy;
    Producer    p;
    Message     msg = message_init();
    char        test[] = "moep";

    message_set_data(msg, test);

    register_dummy_module();
    dummy = lookup_module("dummy");
    p = dummy->producer_init(NULL);
    dummy->produce(p, msg);

    message_free(&msg);
    dummy->producer_free(&p);
    return 0;
}
