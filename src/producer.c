#include "dummy.h"
#include "exports.h"
#include "file.h"
#include "kafka.h"
#include "postgres.h"
#include "producer.h"
#include "redis.h"


Producer
producer_init(char kind, config_setting_t *config)
{
    Producer p;
    config_setting_t *hook_conf;

    switch (kind)
    {
        case 'd':
            p = dummy_producer_init(config);
            break;
        case 'f':
            p = file_producer_init(config);
            break;
        case 'r':
            p = redis_producer_init(config);
            break;
        case 'e':
            p = exports_producer_init(config);
            break;
        case 'p':
            p = postgres_producer_init(config);
            break;
        case 'k':
            p = kafka_producer_init(config);
            break;
        default:
            return NULL;
    }
    p->postget = hook_init();

    if((hook_conf = config_setting_get_member(config,"hooks")) != NULL)
        hooks_add(p->postget,hook_conf);

    return p;
}

void
producer_free(Producer *p)
{
    if ((*p) == NULL)
        return;
    hook_free((*p)->postget);
    (*p)->producer_free(p);
}

void
producer_produce(Producer p, Message msg)
{
    if (p == NULL)
        return;
    p->produce(p, msg);
}
