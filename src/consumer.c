#include "consumer.h"
#include "dummy.h"
#include "file.h"
#include "kafka.h"
#include "redis.h"


Consumer
consumer_init(char kind, config_setting_t *config)
{
    Consumer c;
    config_setting_t *hook_conf;
    switch (kind)
    {
        case 'd':
            c = dummy_consumer_init(config);
            break;
        case 'f':
            c = file_consumer_init(config);
            break;
        case 'r':
            c = redis_consumer_init(config);
            break;
        case 'k':
            c = kafka_consumer_init(config);
            break;
        default:
            return NULL;
    }

    c->preadd = hook_init();

    if((hook_conf = config_setting_get_member(config,"hooks")) != NULL)
        hooks_add(c->preadd,hook_conf);

    return c;
}

void
consumer_free(Consumer *c)
{
    if ((*c) == NULL)
        return;
    hook_free((*c)->preadd);
    (*c)->consumer_free(c);
}

int
consumer_consume(Consumer c, Message msg)
{
    if (c == NULL)
        return -1;
    return c->consume(c, msg);
}
