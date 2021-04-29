#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>

#include "test/test.h"
#include "utils/config.h"
#include "utils/options.h"

static jmp_buf buf;

bool lookup_str(config_t* config, const char *string, const char *test)
{
    const char *retval = NULL;
    if(config_lookup_string(config, string, &retval) != CONFIG_TRUE)
        return false;
    if(strcmp(test,retval))
        return false;
    return true;
}

void _t_group_apply(const char *key, const char *value, void *arg)
{
    const char *elem = NULL;
    config_setting_t *kopt = (config_setting_t *) arg;
    config_setting_lookup_string(kopt,key,&elem);

    if(elem == NULL)
        longjmp(buf,1);
    if(strcmp(value,elem))
        longjmp(buf,1);

    return;
}

int
main(void)
{
    config_t config;
    config_setting_t *kafka_options, *consumer;

    Options o;

    config_init(&config);
    memset(&o, 0, sizeof(o));

    config_read_string(&config,
        "logger={};"
        "producers=();"
        "consumers=({});"
    );
    consumer = config_lookup(&config, "consumers.[0]");

    // test config_set_default_string, config_create_path
    kafka_options = config_create_path(consumer, "kafka_options", CONFIG_TYPE_GROUP);
    config_set_default_string(kafka_options, "enable_auto_commit", "true");
    config_set_default_string(kafka_options, "enable_auto_offset_store", "true");
    config_set_default_string(kafka_options, "auto_commit_interval_ms", "10");
    config_set_default_string(consumer, "topic_options/auto_offset_reset", "latest");


    if(!lookup_str(&config, "consumers.[0].kafka_options.enable_auto_commit", "true"))
        goto error;
    if(!lookup_str(&config, "consumers.[0].kafka_options.enable_auto_offset_store", "true"))
    if(!lookup_str(&config, "consumers.[0].kafka_options.auto_commit_interval_ms", "10"))
        goto error;
    if(!lookup_str(&config, "consumers.[0].topic_options.auto_offset_reset", "latest"))
        goto error;

    // test config_group_apply, set a longjump so we can error out and cleanup
    if(!setjmp(buf))
        config_group_apply(kafka_options,_t_group_apply,kafka_options);
    else
        goto error;

    config_destroy(&config);
    return 0;

    error:
    config_destroy(&config);
    return 1;
}
