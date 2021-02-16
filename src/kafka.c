#include <errno.h>
#include <librdkafka/rdkafka.h>
#include <stdbool.h>
#include <string.h>

#include "kafka.h"
#include "utils/config.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/scalloc.h"


static void
dr_msg_cb (UNUSED rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, UNUSED void *opaque)
{
    if (rkmessage->err)
        logger_log("%s %d: Message delivery failed: %s partition: %d\n", __FILE__, __LINE__, rd_kafka_err2str(rkmessage->err), rkmessage->partition);
}

static void
print_partition_list (const rd_kafka_topic_partition_list_t *partitions)
{
    int i;
    for (i = 0 ; i < partitions->cnt ; i++)
        logger_log("%s %s [%"PRId32"] offset %"PRId64,
            i > 0 ? ",":"",
            partitions->elems[i].topic,
            partitions->elems[i].partition,
            partitions->elems[i].offset);
}

static void
rebalance_cb (rd_kafka_t *rk, rd_kafka_resp_err_t err, rd_kafka_topic_partition_list_t *partitions, UNUSED void *opaque)
{

    logger_log("Consumer group rebalanced:");

    switch (err)
    {
        case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
            logger_log("assigned:");
            print_partition_list(partitions);
            rd_kafka_assign(rk, partitions);
            break;
        case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
            logger_log("revoked:");
            print_partition_list(partitions);
            rd_kafka_assign(rk, NULL);
            break;
        default:
            logger_log("failed: %s\n", rd_kafka_err2str(err));
            rd_kafka_assign(rk, NULL);
        break;
    }
}

/*
 * replace_char
 *      Replace in string `str` character c with character s.
 */
static void replace_char(char *str, char c, char s)
{
    while (*str)
    {
        if (*str == c)
            *str = s;
        ++str;
    }
}

static void kafka_set_option(const char *key, const char *value, void *arg)
{
    char errstr[512];
    char buf[128];
    rd_kafka_conf_t    *conf = (rd_kafka_conf_t *) arg;
    rd_kafka_conf_res_t res;

    strncpy(buf, key, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    /* kafka options use `.` (dot) as a separator */
    replace_char(buf, '_', '.');

    res = rd_kafka_conf_set(conf, buf, value, errstr, sizeof(errstr));
    if (res != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, errstr);
        abort();
    }
}

static void kafka_topic_set_option(const char *key, const char *value, void *arg)
{
    char errstr[512];
    char buf[128];
    rd_kafka_topic_conf_t  *topic_conf = (rd_kafka_topic_conf_t *) arg;
    rd_kafka_conf_res_t     res;

    strncpy(buf, key, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    replace_char(buf, '_', '.');

    res = rd_kafka_topic_conf_set(topic_conf, buf, value, errstr, sizeof(errstr));
    if (res != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, errstr);
        abort();
    }
}

static void kafka_producer_defaults(config_setting_t *c)
{
    config_set_default_string(c, "kafka_options/compression_codec", "lz4");
    config_set_default_string(c, "kafka_options/queue_buffering_max_ms", "1000");
}

static void kafka_consumer_defaults(config_setting_t *c)
{
    config_setting_t *kafka_options;

    kafka_options = config_create_path(c, "kafka_options", CONFIG_TYPE_GROUP);
    if (kafka_options)
    {
        /* enable.auto.commit and enable.auto.offset.store
         * are default values, they're only here to be explicit */
        config_set_default_string(kafka_options, "enable_auto_commit", "true");
        config_set_default_string(kafka_options, "enable_auto_offset_store", "true");
        config_set_default_string(kafka_options, "auto_commit_interval_ms", "10");
    }

    // naively assume you don't want to touch past data
    config_set_default_string(c, "topic_options/auto_offset_reset", "latest");
}

typedef struct Meta {
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    rd_kafka_conf_t *conf;
    rd_kafka_topic_partition_list_t *topics;
} *Meta;

Meta
kafka_producer_meta_init(const char *broker,
                         const char *topic,
                         config_setting_t *kafka_options,
                         config_setting_t *topic_options)
{
    Meta m = SCALLOC(1, sizeof(*m));

    char                    errstr[512];
    rd_kafka_t             *rk;
    rd_kafka_topic_t       *rkt;
    rd_kafka_conf_t        *conf;
    rd_kafka_topic_conf_t  *topic_conf;

    conf = rd_kafka_conf_new();
    config_group_apply(kafka_options, kafka_set_option, conf);

    topic_conf = rd_kafka_topic_conf_new();
    config_group_apply(topic_options, kafka_topic_set_option, topic_conf);
    rd_kafka_conf_set_default_topic_conf(conf, topic_conf);

    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        logger_log("%s %d: Failed to create new producer: %s\n", __FILE__, __LINE__, errstr);
        abort();
    }
    if (rd_kafka_brokers_add(rk, broker) == 0)
    {
        logger_log("%s %d: Failed to add broker: %s\n", __FILE__, __LINE__, broker);
        abort();
    }

    rkt = rd_kafka_topic_new(rk, topic, NULL);

    if (!rkt)
    {
        logger_log("%s %d: Failed to create topic object: %s\n",
            __FILE__,
            __LINE__,
            rd_kafka_err2str(rd_kafka_last_error()));
        rd_kafka_destroy(rk);
        abort();
    }
    m->rk = rk;
    m->rkt = rkt;
    m->conf = conf;
    return m;
}

Meta
kafka_consumer_meta_init(const char *broker,
                         const char *topic,
                         const char *groupid,
                         const config_setting_t *kafka_options,
                         const config_setting_t *topic_options)
{
    Meta m = SCALLOC(1, sizeof(*m));
    char errstr[512];
    rd_kafka_resp_err_t     err;
    rd_kafka_t             *rk;
    rd_kafka_topic_t       *rkt;
    rd_kafka_conf_t        *conf;
    rd_kafka_topic_conf_t  *topic_conf;

    conf = rd_kafka_conf_new();
    config_group_apply(kafka_options, kafka_set_option, conf);

    if (rd_kafka_conf_set(conf, "group.id", groupid, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, errstr);
        abort();
    }

    topic_conf = rd_kafka_topic_conf_new();
    config_group_apply(topic_options, kafka_topic_set_option, topic_conf);
    rd_kafka_conf_set_default_topic_conf(conf, topic_conf);

    rd_kafka_conf_set_rebalance_cb(conf, rebalance_cb);
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        logger_log("%s %d: Failed to create new consumer: %s\n", __FILE__, __LINE__, errstr);
        abort();
    }

    if (rd_kafka_brokers_add(rk, broker) == 0)
    {
        abort();
    }

    rkt = rd_kafka_topic_new(rk, topic, NULL);
    if (!rkt)
    {
        logger_log("%s %d: Failed to create topic object: %s\n", __FILE__, __LINE__, rd_kafka_err2str(rd_kafka_last_error()));
        rd_kafka_destroy(rk);
        abort();
    }

    rd_kafka_poll_set_consumer(rk);

    rd_kafka_topic_partition_list_t *topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, topic, -1);

    if ((err = rd_kafka_subscribe(rk, topics)))
    {
            fprintf(stderr,
                    "%% Failed to start consuming topics: %s\n",
                    rd_kafka_err2str(err));
            abort();
    }
    m->rk = rk;
    m->rkt = rkt;
    m->conf = conf;
    m->topics = topics;
    return m;
}

void
kafka_producer_meta_free(Meta *m)
{
    rd_kafka_flush((*m)->rk, 10*1000);
    rd_kafka_topic_destroy((*m)->rkt);
    rd_kafka_destroy((*m)->rk);
    free(*m);
    *m = NULL;
}

void
kafka_consumer_meta_free(Meta *m)
{
    rd_kafka_flush((*m)->rk, 10*1000);
    rd_kafka_consumer_close((*m)->rk);
    rd_kafka_topic_partition_list_destroy((*m)->topics);
    rd_kafka_topic_destroy((*m)->rkt);
    rd_kafka_destroy((*m)->rk);
    free(*m);
    *m = NULL;
}


Producer
kafka_producer_init(config_setting_t *config)
{
    const char *broker = NULL, *topic = NULL;
    config_setting_t *kafka_options, *topic_options;

    kafka_producer_defaults(config);

    config_setting_lookup_string(config, "broker", &broker);
    config_setting_lookup_string(config, "topic", &topic);
    kafka_options = config_setting_get_member(config, "kafka_options");
    topic_options = config_setting_get_member(config, "topic_options");

    Producer kafka = SCALLOC(1, sizeof(*kafka));

    kafka->meta = kafka_producer_meta_init(broker, topic,
                                           kafka_options,
                                           topic_options);
    kafka->producer_free = kafka_producer_free;
    kafka->produce = kafka_producer_produce;

    return kafka;
}

void
kafka_producer_produce(Producer p, Message msg)
{
    char *buf = (char *) message_get_data(msg);
    size_t len = message_get_len(msg);
    rd_kafka_t *rk = ((Meta) p->meta)->rk;
    rd_kafka_topic_t *rkt = ((Meta)p->meta)->rkt;
retry:
    if (rd_kafka_produce(
                rkt,
                RD_KAFKA_PARTITION_UA,
                RD_KAFKA_MSG_F_COPY,
                buf, len,
                NULL, 0,
                NULL) == -1)
    {
        if (rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL)
        {
            rd_kafka_poll(rk, 10*1000);
            goto retry;
        }
        else
        {
            logger_log(
                "%s %d Failed to produce to topic %s: %s\n",
                __FILE__, __LINE__,
                rd_kafka_topic_name(rkt),
                rd_kafka_err2str(rd_kafka_last_error())
            );
        }
    }
    rd_kafka_poll(rk, 0);
}

void
kafka_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);
    kafka_producer_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
kafka_consumer_init(config_setting_t *config)
{
    const char *broker = NULL, *topic = NULL, *groupid = NULL;
    config_setting_t *kafka_options, *topic_options;

    kafka_consumer_defaults(config);

    config_setting_lookup_string(config, "broker", &broker);
    config_setting_lookup_string(config, "topic", &topic);
    config_setting_lookup_string(config, "groupid", &groupid);
    kafka_options = config_setting_get_member(config, "kafka_options");
    topic_options = config_setting_get_member(config, "topic_options");

    Consumer kafka = SCALLOC(1, sizeof(*kafka));

    kafka->meta = kafka_consumer_meta_init(broker, topic, groupid,
                                           kafka_options,
                                           topic_options);
    kafka->consumer_free = kafka_consumer_free;
    kafka->consume = kafka_consumer_consume;

    return kafka;
}

void _rkmessage_log(rd_kafka_message_t *rkmessage)
{
    if (rkmessage->rkt)
    {
        logger_log("%% Consume error for "
            "topic \"%s\" [%"PRId32"] "
            "offset %"PRId64": %s\n",
            rd_kafka_topic_name(rkmessage->rkt),
            rkmessage->partition,
            rkmessage->offset,
            rd_kafka_message_errstr(rkmessage));
    }
    else
        logger_log("%% Consumer error: %s: %s\n",
            rd_kafka_err2str(rkmessage->err),
            rd_kafka_message_errstr(rkmessage));
    return;
}

int
kafka_consumer_consume(Consumer c, Message msg)
{
    rd_kafka_t *rk = ((Meta) c->meta)->rk;
    rd_kafka_message_t *rkmessage;

    rkmessage = rd_kafka_consumer_poll(rk, 10000);

    if (!rkmessage)
        return 0;

    if (rkmessage->err)
    {
        switch(rkmessage->err)
        {
            case RD_KAFKA_RESP_ERR__PARTITION_EOF:
                break;

            case RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION:
            case RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC:
                _rkmessage_log(rkmessage);
                abort();
                break;

            default:
                _rkmessage_log(rkmessage);
        }
        rd_kafka_message_destroy(rkmessage);
        return 0;
    }

    char *cpy = SCALLOC((int)rkmessage->len + 1, sizeof(*cpy));
    memcpy(cpy, (char *)rkmessage->payload, (size_t)rkmessage->len);
    message_set_data(msg, cpy);
    message_set_len(msg, (size_t)rkmessage->len);
    rd_kafka_message_destroy(rkmessage);

    return 0;
}

void
kafka_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    kafka_consumer_meta_free(&m);
    free(*c);
    *c = NULL;
}

bool
kafka_validator(config_setting_t *config)
{
    const char *result = NULL;
    if(!CONF_L_IS_STRING(config, "broker", &result, "kafka: need a broker!"))
        return false;
    result = NULL;
    if(!CONF_L_IS_STRING(config, "topic", &result, "kafka: need a topic!"))
        return false;

    return true;
}

bool
kafka_producer_validator(config_setting_t *config)
{
    return kafka_validator(config);
}

bool
kafka_consumer_validator(config_setting_t *config)
{
    const char *groupid = NULL;
    if(!CONF_L_IS_STRING(config, "groupid", &groupid,
        "kafka: consumer needs a group!"))
        return false;

    return kafka_validator(config);
}

Validator
kafka_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = &kafka_consumer_validator;
    v->validate_producer = &kafka_producer_validator;

    return(v);
}
