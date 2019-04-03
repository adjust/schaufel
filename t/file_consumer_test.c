#include <queue.h>
#include <consumer.h>
#include <stdio.h>
#include <test/test.h>
#include <utils/logger.h>
#include <utils/helper.h>
#include <utils/config.h>
#include <utils/options.h>

int
main(void)
{
    config_t config;
    config_setting_t *croot, *logger, *file, *setting;
    config_init(&config);
    croot = config_root_setting(&config);

    //logger
    logger = config_setting_add(croot,"logger",CONFIG_TYPE_GROUP);
    setting = config_setting_add(logger, "file", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, "sample/dummy_log");
    //file
    file = config_setting_add(croot,"file",CONFIG_TYPE_GROUP);
    setting = config_setting_add(file, "file", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, "sample/dummy_file");

    logger_init(logger);
    Message msg = message_init();
    Consumer c = consumer_init('f', file);
    char *string;

    consumer_consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "first", 5) == 0);
    free(string);
    consumer_consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "second", 6) == 0);
    free(string);
    consumer_consume(c, msg);
    string =(char *) message_get_data(msg);
    pretty_assert(string != NULL);
    if (string != NULL)
        pretty_assert(strncmp(string, "third", 5) == 0);
    free(string);

    message_free(&msg);
    consumer_free(&c);
    logger_free();
    return 0;
}
