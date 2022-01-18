#include "schaufel.h"
#include "test/test.h"
#include "utils/config.h"

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
    const char *type, *file, *facility, *ident;
    config_setting_t *logger = NULL;;
    int mode;

    // test full file definition
    logger = _config_init(&root);
    logger_parse("FILE:0644:foobar",logger);
    config_lookup_string(&root, "logger/type", &type);
    config_lookup_string(&root, "logger/file", &file);
    config_lookup_int(&root, "logger/mode", &mode);
    pretty_assert(strncmp(type,"file",4) == 0);
    pretty_assert(strncmp(file,"foobar",6) == 0);
    pretty_assert(mode == 0644);

    logger = _config_init(&root);
    logger_parse("FILE:habicht",logger);
    config_lookup_string(&root, "logger/type", &type);
    config_lookup_string(&root, "logger/file", &file);
    config_lookup_int(&root, "logger/mode", &mode);
    pretty_assert(strncmp(type,"file",4) == 0);
    pretty_assert(strncmp(file,"habicht",7) == 0);

    logger = _config_init(&root);
    logger_parse("FILE",logger);
    config_lookup_string(&root, "logger/type", &type);
    config_lookup_string(&root, "logger/file", &file);
    config_lookup_int(&root, "logger/mode", &mode);
    pretty_assert(strncmp(type,"file",4) == 0);
    pretty_assert(strncmp(file,"FILE",4) == 0);

    logger = _config_init(&root);
    logger_parse("SYSLOG",logger);
    config_lookup_string(&root, "logger/type", &type);
    pretty_assert(strncmp(type,"syslog",6) == 0);

    logger = _config_init(&root);
    logger_parse("SYSLOG:schaufel_ident:user",logger);
    config_lookup_string(&root, "logger/type", &type);
    pretty_assert(strncmp(type,"syslog",6) == 0);
    config_lookup_string(&root, "logger/facility", &facility);
    pretty_assert(strncmp(facility,"user",6) == 0);
    config_lookup_string(&root, "logger/ident", &ident);
    pretty_assert(strncmp(ident,"schaufel_ident", 12) == 0);

    logger = _config_init(&root);
    logger_parse("SYSLOG:daemon",logger);
    config_lookup_string(&root, "logger/type", &type);
    pretty_assert(strncmp(type,"syslog",6) == 0);
    config_lookup_string(&root, "logger/facility", &facility);
    pretty_assert(strncmp(facility,"daemon",6) == 0);

    logger = _config_init(&root);
    logger_parse("NULL",logger);
    config_lookup_string(&root, "logger/type", &type);
    pretty_assert(strncmp(type,"null",6) == 0);

    config_destroy(&root);
    return 0;
}
