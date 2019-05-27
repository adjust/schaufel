#include <stdio.h>

#include "file.h"
#include "modules.h"
#include "queue.h"
#include "test/test.h"
#include "utils/config.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/options.h"

int
main(void)
{
    config_t config;
    config_setting_t *croot, *logger, *file_setting, *setting;
    char *string;

    config_init(&config);
    croot = config_root_setting(&config);

    //logger
    logger = config_setting_add(croot,"logger",CONFIG_TYPE_GROUP);
    setting = config_setting_add(logger, "file", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, "sample/dummy_log");
    //file
    file_setting = config_setting_add(croot,"file",CONFIG_TYPE_GROUP);
    setting = config_setting_add(file_setting, "file", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, "sample/dummy_file");

    register_file_module();
    ModuleHandler *file = lookup_module("file");
    Consumer c = file->consumer_init(file_setting);

    logger_init(logger);
    Message msg = message_init();
    file->consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "first", 5) == 0);
    free(string);
    file->consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "second", 6) == 0);
    free(string);
    file->consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "third", 5) == 0);
    free(string);

    message_free(&msg);
    file->consumer_free(&c);
    config_destroy(&config);
    logger_free();
    return 0;
}
