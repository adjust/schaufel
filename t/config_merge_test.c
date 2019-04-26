#include <stdio.h>
#include <test/test.h>
#include <utils/config.h>
#include <utils/options.h>
#include <stdbool.h>

bool lookup_str(config_t* config, const char *string, const char *test)
{
    const char *retval = NULL;
    if(config_lookup_string(config, string, &retval) != CONFIG_TRUE)
        return false;
    if(strcmp(test,retval))
        return false;
    return true;
}

bool lookup_int(config_t* config, const char *string, int test)
{
    int retval = 0;
    if(config_lookup_int(config, string, &retval) != CONFIG_TRUE)
        return false;
    if(retval != test)
        return false;
    return true;
}

int
main(void)
{
    config_t config;

    Options o;

    config_init(&config);
    memset(&o, 0, sizeof(o));

    config_read_string(&config,
        "logger={};"
        "producers=();"
        "consumers=();"
    );

    o.logger = "sample/dummy.log";
    o.consumer_threads = 5;
    o.producer_threads = 6;
    o.input = 'd';
    o.output = 'f';
    o.in_file = "sample/dummy_file";
    o.out_file = "sample/dummy_file_2";
    o.in_pipeline = 20;
    o.out_pipeline = 21;
    o.in_host = "localhost:5432";
    o.out_host = "localhost:5433";
    o.in_broker = "zookeeper-1";
    o.out_broker = "zookeeper-2";
    o.in_groupid = "testgroup";
    o.out_groupid = "testgroup2";
    o.in_topic = "testtopic";
    o.out_topic = "testtopic2";

    config_merge(&config, o);

    if(!lookup_str(&config, "logger.type", "file"))
        return 1;
    if(!lookup_str(&config, "logger.file", "sample/dummy.log"))
        return 1;
    if(!lookup_int(&config, "consumers.[0].threads", 5))
        return 1;
    if(!lookup_int(&config, "producers.[0].threads", 6))
        return 1;
    if(!lookup_str(&config, "consumers.[0].type", "dummy"))
        return 1;
    if(!lookup_str(&config, "producers.[0].type", "file"))
        return 1;
    if(!lookup_str(&config, "consumers.[0].file", "sample/dummy_file"))
        return 1;
    if(!lookup_str(&config, "producers.[0].file", "sample/dummy_file_2"))
        return 1;
    if(!lookup_int(&config, "consumers.[0].pipeline", 20))
        return 1;
    if(!lookup_int(&config, "producers.[0].pipeline", 21))
        return 1;
    if(!lookup_str(&config, "consumers.[0].host", "localhost:5432"))
        return 1;
    if(!lookup_str(&config, "producers.[0].host", "localhost:5433"))
        return 1;
    if(!lookup_str(&config, "consumers.[0].broker", "zookeeper-1"))
        return 1;
    if(!lookup_str(&config, "producers.[0].broker", "zookeeper-2"))
        return 1;
    if(!lookup_str(&config, "consumers.[0].groupid", "testgroup"))
        return 1;
    if(!lookup_str(&config, "producers.[0].groupid", "testgroup2"))
        return 1;
    if(!lookup_str(&config, "consumers.[0].topic", "testtopic"))
        return 1;
    if(!lookup_str(&config, "producers.[0].topic", "testtopic2"))
        return 1;

    config_destroy(&config);
    return 0;
}
