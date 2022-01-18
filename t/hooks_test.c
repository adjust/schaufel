#include "schaufel.h"
#include "test/test.h"
#include "hooks.h"
#include "queue.h"

int main()
{
    int res = 0;
    config_t root;
    config_setting_t *hook_conf = NULL;
    config_init(&root);
    res = config_read_string(&root, "hooks = ({ type = \"dummy\"; });");
    pretty_assert(res == CONFIG_TRUE);
    hook_conf = config_lookup(&root,"hooks");

    // test finding a hook
    hooks_register();
    pretty_assert(hooks_validate(hook_conf) == true);

    Hooklist test = hook_init();
    hooks_add(test,hook_conf);

    Message msg = message_init();
    message_set_data(msg,"hurz");
    message_set_len(msg,4);

    pretty_assert(hooklist_run(test,msg) == true);

    free(msg);
    hook_free(test);
    hooks_deregister();

    config_destroy(&root);
    return 0;
}
