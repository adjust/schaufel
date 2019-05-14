#include <stdio.h>
#include <string.h>

#include "dummy.h"
#include "modules.h"
#include "queue.h"
#include "test/test.h"


int
main(void)
{
    ModuleHandler *dummy;
    Consumer    c;
    Message     msg;
    char       *string;

    register_dummy_module();
    msg = message_init();

    dummy = lookup_module("dummy");
    c = dummy->consumer_init(NULL);
    dummy->consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
    {
        pretty_assert(strncmp(string, "{\"type\":\"dummy\"}", 16) == 0);
        printf("%s\n", string);
    }

    message_free(&msg);
    free(string);
    dummy->consumer_free(&c);
    return 0;
}
