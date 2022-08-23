#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "test/test.h"
#include "utils/config.h"
#include "utils/options.h"
#include "kafka.h"
#include "validator.h"

bool lookup_str(config_t* config, const char *string, const char *test)
{
    const char *retval = NULL;
    if(config_lookup_string(config, string, &retval) != CONFIG_TRUE)
        return false;
    if(strcmp(test,retval))
        return false;
    return true;
}

int
main(void)
{
    config_t config;
    config_setting_t *consumer;

    Options o;

    config_init(&config);
    memset(&o, 0, sizeof(o));

    config_read_string(&config,
        "consumers=({"
        "type=\"kafka\";"
        "threads=1;"
        "groupid=\"test.group\";"
        "topic=\"test.topic:0,2-5,7\";"
        "broker=\"test-broker\";"
        "});"
    );
    consumer = config_lookup(&config, "consumers.[0]");

    Validator kv = kafka_validator_init();
    pretty_assert(kv->validate_consumer(consumer) == true);

    // test config_set_default_string, config_create_path
    bool res = true, ret;
    pretty_assert((ret = lookup_str(&config, "consumers.[0].kafka_options.enable_auto_commit", "false")));
    if(!ret) res = false;
    pretty_assert((ret = lookup_str(&config, "consumers.[0].topic", "test.topic")));
    if(!ret) res = false;
    pretty_assert((ret = lookup_str(&config, "consumers.[0].partitions", "0,2-5,7")));
    if(!ret) res = false;

    free(kv);
    config_destroy(&config);

    // if your bool does not actually define 1 as true, don't call me
    return !res;
}
