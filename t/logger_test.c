#include "schaufel.h"
#include <unistd.h>
#include <sys/stat.h>

#include "test/test.h"
#include "utils/config.h"
#include "utils/logger.h"

config_setting_t *_config_init(config_t *root)
{
    if(!root)
        config_init(root);
    else {
        config_destroy(root);
        config_init(root);
    }
    config_read_string(root, "logger: {};");
    return config_lookup(root, "logger");
}

int main()
{
    config_t root = {0};
    config_setting_t *logger = NULL;;

    // test full file definition
    logger = _config_init(&root);
    logger_parse("FILE:0644:./schaufel_logtest",logger);
    pretty_assert(logger_validate(logger) == true);
    logger = config_lookup(&root,"logger");
    logger_init(logger);
    logger_log("testmessage file 1");
    // We can't actually compare mode because
    // umask might influence result
    unlink("./schaufel_logtest");

    logger_free();

    // test file
    logger = _config_init(&root);
    logger_parse("./schaufel_logtest2",logger);
    pretty_assert(logger_validate(logger) == true);
    logger = config_lookup(&root,"logger");
    logger_init(logger);
    logger_log("testmessage file 2");
    unlink("./schaufel_logtest2");
    logger_free();

    // stderr (not a test)
    logger = _config_init(&root);
    logger_parse("STDERR",logger);
    pretty_assert(logger_validate(logger) == true);
    logger = config_lookup(&root,"logger");
    logger_init(logger);
    logger_log("testmessage stderr");
    logger_free();

    // stdout (not a test)
    logger = _config_init(&root);
    logger_parse("STDOUT",logger);
    pretty_assert(logger_validate(logger) == true);
    logger = config_lookup(&root,"logger");
    logger_init(logger);
    logger_log("testmessage stdout");
    logger_free();

    // null (not a test)
    logger = _config_init(&root);
    logger_parse("NULL",logger);
    pretty_assert(logger_validate(logger) == true);
    logger = config_lookup(&root,"logger");
    logger_init(logger);
    logger_log("testmessage null");
    logger_free();

    // syslog
    logger = _config_init(&root);
    logger_parse("SYSLOG",logger);
    pretty_assert(logger_validate(logger) == true);
    logger = config_lookup(&root,"logger");
    logger_init(logger);
    logger_log("testmessage syslog 1");
    logger_free();

    // syslog
    logger = _config_init(&root);
    logger_parse("SYSLOG:schaufel_test:daemon",logger);
    pretty_assert(logger_validate(logger) == true);
    logger = config_lookup(&root,"logger");
    logger_init(logger);
    logger_log("testmessage syslog 2 ");
    logger_free();

    config_destroy(&root);
    return 0;
}
