#include <errno.h>
#include <librdkafka/rdkafka.h>
#include <stdbool.h>
#include <string.h>
#include <regex.h>

#include "kafka.h"
#include "utils/config.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/scalloc.h"

/*
 *  this function is meant as a callback
 *  for metadata based transactions
 */
static bool consumer_commit_offset(Message msg)
{
    rd_kafka_message_t *rkm;
    rd_kafka_resp_err_t resp_err;

    Metadata *md = message_get_metadata(msg);
    MDatum rk_message = metadata_find(md, "rk_message");
    if(!rk_message)
    {
        logger_log("%s %d: FATAL transactional message but no "
            "rdkafka envelope", __FILE__, __LINE__);
        abort();
    }

    /* nothing left to do here */
    if(rk_message == NULL || rk_message->type != MTYPE_OPAQUE)
        goto error;

    rkm = (rd_kafka_message_t *) rk_message->value.ptr;
    resp_err = rd_kafka_offset_store(rkm->rkt, rkm->partition, rkm->offset);

    /* bail out to not store any later offsets */
    if(resp_err)
    {
        logger_log("%s %d: FATAL failed to store offsets: %s",
            __FILE__, __LINE__, rd_kafka_err2str(resp_err));
        abort();
    }

    rd_kafka_message_destroy(rkm);
    rk_message->value.ptr = NULL;
    // we never owned this pointer in the first place
    // rkm owned message data, give it up
    message_set_data(msg, NULL);

    return true;

    error:
    return false;
}

typedef struct Meta {
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    rd_kafka_conf_t *conf;
    rd_kafka_topic_partition_list_t *topics;
    rd_kafka_queue_t *rkqu;
    int transactional;
} *Meta;

static void
dr_msg_cb (UNUSED rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
    const char *broker = opaque;
    if (rkmessage->err)
        logger_log("%s %d: %s: Message delivery failed: %s partition: %d\n",
        __FILE__, __LINE__, broker,
        rd_kafka_err2str(rkmessage->err), rkmessage->partition);
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
err_cb (rd_kafka_t *rk, rd_kafka_resp_err_t err, const char *reason, void *opaque)
{
    const char *broker = opaque;
    if (err == RD_KAFKA_RESP_ERR__FATAL) {
        char errstr[512];
        err = rd_kafka_fatal_error(rk, errstr, sizeof(errstr));
        logger_log("%s %d: %s: FATAL ERROR CALLBACK: %s: %s: %s\n",
            __FILE__, __LINE__, broker,
            rd_kafka_name(rk), rd_kafka_err2str(err), errstr);
    } else {
        logger_log("%s %d: %s: ERROR CALLBACK: %s: %s: %s\n",
            __FILE__, __LINE__, broker,
            rd_kafka_name(rk), rd_kafka_err2str(err), reason);
    }
}

static void
offset_commit_cb(UNUSED rd_kafka_t *rk, rd_kafka_resp_err_t err, UNUSED rd_kafka_topic_partition_list_t *partitions, void *opaque)
{
    const char *broker = opaque;

    // Not considered an error, no offset to commit
    if (err == RD_KAFKA_RESP_ERR__NO_OFFSET)
        return;

    if (err)
    {
        logger_log("%s %d: %s: OFFSET COMMIT CALLBACK: %s\n",
            __FILE__, __LINE__, broker,
            rd_kafka_err2str(err));
    }
}

static void
rebalance_cb (rd_kafka_t *rk, rd_kafka_resp_err_t err, rd_kafka_topic_partition_list_t *partitions, void *opaque)
{

    const char *broker = opaque;
    logger_log("%s %d: %s: Consumer group rebalanced:",
        __FILE__, __LINE__, broker);

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

static int32_t *explode_partitions(const char *parts)
{
    int32_t *p = SCALLOC(sizeof(p),8192);
    int32_t *start = p;
    if(!parts)
    {
        logger_log("%s %d: partition list undefined!", __FILE__, __LINE__);
        abort();
    }
    int err = errno;
    uint32_t count = 0;
    errno = 0;

    while(*parts)
    {
        uint64_t a = 0, b = 0;
        char *endptr = NULL;

        a = strtoul(parts, &endptr, 0);
        if(*endptr == '-')
        {
            b = strtoul(++endptr, &endptr, 0);
        }
        else if(endptr == parts)
        {
            logger_log("%s %d: invalid token: %s",
                __FILE__, __LINE__, parts);
             abort();
        }
        else
            b = a;

        if(a > b)
        {
            a = a+b;
            b = a-b;
            a = a-b;
        }

        if(errno)
        {
            logger_log("%s %d: partition out of range %s",
                __FILE__, __LINE__, parts);
            abort();
        }

        while(a <= b)
        {
            *p = a++;
            p++;
            count++;
            if(count > 8190)
            {
                logger_log("%s %d: manual assignment of more than 8000 "
                    "partitions? Did you mean to do that?",
                    __FILE__, __LINE__);
                abort();
            }
        }
        parts = endptr;
    }
    *p = -1;
    errno = err;

    return start;
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
        config_set_default_string(kafka_options, "auto_commit_interval_ms", "5000");
    }

    // naively assume you don't want to touch past data
    config_set_default_string(c, "topic_options/auto_offset_reset", "latest");
}

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
    rd_kafka_conf_set_opaque(conf, (void *) broker);
    config_group_apply(kafka_options, kafka_set_option, conf);

    topic_conf = rd_kafka_topic_conf_new();
    config_group_apply(topic_options, kafka_topic_set_option, topic_conf);
    rd_kafka_conf_set_default_topic_conf(conf, topic_conf);

    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        logger_log("%s %d: %s: Failed to create new producer: %s\n",
            __FILE__, __LINE__, broker, errstr);
        abort();
    }
    if (rd_kafka_brokers_add(rk, broker) == 0)
    {
        logger_log("%s %d: %s: Failed to add broker: %s\n",
            __FILE__, __LINE__, broker, broker);
        abort();
    }

    rkt = rd_kafka_topic_new(rk, topic, NULL);

    if (!rkt)
    {
        logger_log("%s %d: %s: Failed to create topic object: %s\n",
            __FILE__, __LINE__, broker,
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
                         const int32_t *partarray,
                         const char *groupid,
                         const config_setting_t *kafka_options,
                         const config_setting_t *topic_options,
                         int transactional)
{
    Meta m = SCALLOC(1, sizeof(*m));
    char errstr[512];
    const int32_t          *partcopy = partarray;
    rd_kafka_resp_err_t     err;
    rd_kafka_t             *rk;
    rd_kafka_topic_t       *rkt;
    rd_kafka_conf_t        *conf;
    rd_kafka_topic_conf_t  *topic_conf;
    rd_kafka_queue_t       *rkqu = NULL;

    conf = rd_kafka_conf_new();
    rd_kafka_conf_set_opaque(conf, (void *) broker);
    config_group_apply(kafka_options, kafka_set_option, conf);

    if (groupid && rd_kafka_conf_set(conf, "group.id", groupid, errstr,
        sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s: %s", __FILE__, __LINE__, broker, errstr);
        abort();
    }

    topic_conf = rd_kafka_topic_conf_new();
    config_group_apply(topic_options, kafka_topic_set_option, topic_conf);
    rd_kafka_conf_set_default_topic_conf(conf, topic_conf);

    rd_kafka_conf_set_offset_commit_cb(conf, offset_commit_cb);
    rd_kafka_conf_set_rebalance_cb(conf, rebalance_cb);
    rd_kafka_conf_set_error_cb(conf, err_cb);

    rd_kafka_conf_set(conf, "metadata.broker.list",
        broker, errstr, sizeof(errstr));

    rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        logger_log("%s %d: %s: Failed to create new consumer: %s\n",
            __FILE__, __LINE__, broker, errstr);
        abort();
    }

    rkt = rd_kafka_topic_new(rk, topic, NULL);
    if (!rkt)
    {
        logger_log("%s %d: %s: Failed to create topic object: %s\n",
            __FILE__, __LINE__, broker, rd_kafka_err2str(rd_kafka_last_error()));
        rd_kafka_destroy(rk);
        abort();
    }

    rd_kafka_poll_set_consumer(rk);

    rd_kafka_topic_partition_list_t *topics = rd_kafka_topic_partition_list_new(1);
    if(partarray) /* Simple Consumer */
    {
        rkqu = rd_kafka_queue_new(rk);
        while((*partarray) > -1)
        {
            rd_kafka_topic_partition_list_add(topics, topic, *partarray);
            rd_kafka_consume_start_queue(rkt, *partarray, RD_KAFKA_OFFSET_END, rkqu);
            partarray++;
        }
        partarray = partcopy;
    }
    else
        rd_kafka_topic_partition_list_add(topics, topic, -1);

    /* High level Consumer */
    if (groupid && (err = rd_kafka_subscribe(rk, topics)))
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
    m->rkqu = rkqu;
    m->transactional = transactional;
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
    if((*m)->rkqu)
        rd_kafka_queue_destroy((*m)->rkqu);
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
    rd_kafka_headers_t *hdrs = (rd_kafka_headers_t *) message_get_headers(msg);
    size_t len = message_get_len(msg);
    rd_kafka_t *rk = ((Meta) p->meta)->rk;
    rd_kafka_topic_t *rkt = ((Meta)p->meta)->rkt;
retry:
    if (hdrs) {
        rd_kafka_headers_t *hdrs_copy = rd_kafka_headers_copy(hdrs);
        if(rd_kafka_producev(
                rk,
                RD_KAFKA_V_RKT(rkt),
                RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                RD_KAFKA_V_VALUE(buf, len),
                RD_KAFKA_V_HEADERS(hdrs_copy),
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
    } else {
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
    int transactional = 0;
    config_setting_t *kafka_options, *topic_options, *kpart;
    int32_t *partarray = NULL;

    kafka_consumer_defaults(config);

    config_setting_lookup_string(config, "broker", &broker);
    config_setting_lookup_string(config, "topic", &topic);
    config_setting_lookup_string(config, "groupid", &groupid);

    if((kpart = config_setting_lookup(config, "partitions")))
        partarray = explode_partitions(config_setting_get_string(kpart));

    config_setting_lookup_bool(config, "transactional", &transactional);

    kafka_options = config_setting_get_member(config, "kafka_options");
    topic_options = config_setting_get_member(config, "topic_options");

    Consumer kafka = SCALLOC(1, sizeof(*kafka));

    kafka->meta = kafka_consumer_meta_init(broker, topic, partarray, groupid,
                                           kafka_options,
                                           topic_options,
                                           transactional
                                           );
    kafka->consumer_free = kafka_consumer_free;
    if(groupid && !transactional)
        kafka->consume = kafka_consumer_consume;
    else if (transactional)
        kafka->consume = kafka_transactional_consumer_consume;
    else
        kafka->consume = kafka_simple_consumer_consume;

    free(partarray);
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
kafka_simple_consumer_consume(Consumer c, Message msg)
{
    rd_kafka_queue_t *rkqu = ((Meta) c->meta)->rkqu;
    rd_kafka_message_t *rkmessage;
    rd_kafka_headers_t *hdrs;

    rkmessage = rd_kafka_consume_queue(rkqu, 10000);

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

    // Here we don't detach the headers so the memory gets cleared when
    // the message is destroyed.
    if (!rd_kafka_message_headers(rkmessage, &hdrs)) {
        if (hdrs){
            rd_kafka_headers_t *hdrs_copy = rd_kafka_headers_copy(hdrs);
            message_set_headers(msg, hdrs_copy);
        }
    }

    char *cpy = SCALLOC((int)rkmessage->len + 1, sizeof(*cpy));
    memcpy(cpy, (char *)rkmessage->payload, (size_t)rkmessage->len);
    message_set_data(msg, cpy);
    message_set_len(msg, (size_t)rkmessage->len);
    rd_kafka_message_destroy(rkmessage);

    return 0;
}

int
kafka_transactional_consumer_consume(Consumer c, Message msg)
{
    rd_kafka_t *rk = ((Meta) c->meta)->rk;
    rd_kafka_message_t *rkmessage;
    rd_kafka_headers_t *hdrs;

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

    // Here we don't detach the headers so the memory gets cleared when
    // the message is destroyed.
    if (!rd_kafka_message_headers(rkmessage, &hdrs)) {
        if (hdrs){
            rd_kafka_headers_t *hdrs_copy = rd_kafka_headers_copy(hdrs);
            message_set_headers(msg, hdrs_copy);
        }
    }

    message_set_data(msg, rkmessage->payload);
    message_set_len(msg, (size_t)rkmessage->len);

    /* Provide callback functionality:
     *  - callback function for commiting offsets
     *  - rk_message * envelope for ocmmit offsets
     */
    Datum cb, rkm;
    cb.func = &consumer_commit_offset;
    rkm.ptr = rkmessage;

    Metadata *md = message_get_metadata(msg);

    MDatum rk_callback = mdatum_init(MTYPE_FUNC,
        cb, sizeof(void *));
    if(!rk_callback)
    {
        logger_log("%s %d: Failed to alloc!",
            __FILE__, __LINE__);
        abort();
    }
    metadata_insert(md,"callback", rk_callback);

    MDatum rk_message = mdatum_init(MTYPE_OPAQUE,
        rkm, sizeof(void *));
    if(!rk_message)
    {
        logger_log("%s %d: Failed to alloc!",
            __FILE__, __LINE__);
        abort();
    }
    metadata_insert(md,"rk_message", rk_message);

    return 0;
}

int
kafka_consumer_consume(Consumer c, Message msg)
{
    rd_kafka_t *rk = ((Meta) c->meta)->rk;
    rd_kafka_message_t *rkmessage;
    rd_kafka_headers_t *hdrs;

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

    // Here we don't detach the headers so the memory gets cleared when
    // the message is destroyed.
    if (!rd_kafka_message_headers(rkmessage, &hdrs)) {
        if (hdrs){
            rd_kafka_headers_t *hdrs_copy = rd_kafka_headers_copy(hdrs);
            message_set_headers(msg, hdrs_copy);
        }
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
    regex_t top_par = {0};
    regmatch_t pmatch[20] = {0};
    int res = 0;

    if(!CONF_L_IS_STRING(config, "broker", &result, "kafka: need a broker!"))
        return false;
    result = NULL;

    // todo: this is a bug, causes segfault if topic doesn't exist
    if(config_setting_type(config_setting_lookup(config, "topic")) ==
        CONFIG_TYPE_LIST)
    {
        // not implemented
        goto error;
    }
    if(!CONF_L_IS_STRING(config, "topic", &result, "kafka: need a topic!"))
        goto error;

    /*  grammar of kafka names:
     *  ^(
     *      [[:alpha:]._-]+
     *   )
     *   (:
     *      ((
     *          [0-9]+
     *          (
     *              -[0-9]+
     *          )?,
     *       ?)+)
     *   )?
     *  $
     *  kafka takes an alphanumeric string with .-_
     *  optionally, we can specify partion numbers after :
     *  e.g.
     *  kafka.topic.events.3:0-5,7,9,13
     */

    res = regcomp(&top_par,
        "^([[:alnum:]._-]+)(:(([0-9]+(-[0-9]+)?,?)+))?$", REG_EXTENDED);

    if(res)
    {
        char buf[512] = {0};
        regerror(res,&top_par,buf,sizeof(buf));
        fprintf(stderr, "%s %d: compiling kafka topic/partition regex failed: %s\n",
            __FILE__, __LINE__, buf);

        regfree(&top_par);
        goto error;
    }

    if(regexec(&top_par, result, sizeof(pmatch)/sizeof(pmatch[0]),
        pmatch, 0) == REG_NOMATCH)
    {
        fprintf(stderr, "%s %d: %s isn't a kafka topic/partition name\n",
            __FILE__, __LINE__, result);

        regfree(&top_par);
        goto error;
    }
    regfree(&top_par);
    // we have found a partition specifier
    if(pmatch[2].rm_so > 0)
    {
        // Truncate the string in the hackiest way possible
        *(char *) (result+pmatch[2].rm_so) = '\0';

        config_setting_t *partitions = NULL;
        if((partitions = config_setting_get_member(config, "partitions")))
        {
            fprintf(stderr, "%s %d: partitions already specified in topic"
                " with partitions\n", __FILE__, __LINE__);
            goto error;
        }
        partitions = config_setting_add(config, "partitions", CONFIG_TYPE_STRING);
        if(!partitions)
        {
            fprintf(stderr, "%s %d: failed to alloc config setting!\n",
                __FILE__, __LINE__);
            abort();
        }
        config_setting_set_string(partitions, result+pmatch[3].rm_so);

        /* The high level consumer will rebalance */
        fprintf(stderr, "%s %d: Manual partition assignment found. "
            "Disabling librdkafka high level consumer\n", __FILE__, __LINE__);

        config_setting_t *kopts = NULL, *auto_commit = NULL;
        if(!(kopts = config_setting_lookup(config, "kafka_options")))
        {
            kopts = config_setting_add(config, "kafka_options", CONFIG_TYPE_GROUP);
            auto_commit = config_setting_add(kopts,
                "enable_auto_commit", CONFIG_TYPE_STRING);
            config_setting_set_string(auto_commit, "false");
        }
        else if(config_setting_type(kopts) != CONFIG_TYPE_GROUP)
        {
            fprintf(stderr, "%s %d: kafka_options must be a group type!\n",
                __FILE__, __LINE__);
            goto error;
        }
        else
        {
            auto_commit = config_setting_get_member(kopts, "enable_auto_commit");
            if(!auto_commit) {
                auto_commit =  config_setting_add(kopts, "enable_auto_commit",
                    CONFIG_TYPE_STRING);
            }
            config_setting_set_string(auto_commit, "false");
        }
    }

    config_setting_t *partitions = NULL;
    if((partitions = config_setting_get_member(config, "partitions")))
    {
        if(config_setting_type(partitions) == CONFIG_TYPE_STRING)
        {
            const char *p = config_setting_get_string(partitions);
            regex_t par;

            res = regcomp(&par, "^([0-9]+(-[0-9]+)?,?)+?$",
                REG_EXTENDED | REG_NOSUB);

            if(res)
            {
                char buf[512] = {0};
                regerror(res,&par,buf,sizeof(buf));
                fprintf(stderr, "%s %d: compiling kafka partition regex failed:"
                    " %s\n", __FILE__, __LINE__, buf);
                regfree(&par);

                goto error;
            }
            if(regexec(&par, p, 0, NULL, 0) == REG_NOMATCH)
            {
                fprintf(stderr, "%s %d: invalid partition string: %s\n",
                    __FILE__, __LINE__, p);
                regfree(&par);
                goto error;
            }
            regfree(&par);
        }
        else if(config_setting_type(partitions) == CONFIG_TYPE_INT)
        {
            const int32_t value = config_setting_get_int(partitions);
            if(value < 0)
            {
                fprintf(stderr, "%s %d: partitions must be a positive integer!\n",
                    __FILE__, __LINE__);
                goto error;
            }
            config_setting_remove(config, "partitions");

            partitions = config_setting_add(config, "partitions", CONFIG_TYPE_STRING);
            if(!partitions)
            {
                fprintf(stderr, "%s %d: failed to alloc config setting!\n",
                    __FILE__, __LINE__);
                abort();
            }

            char buf[512];
            snprintf(buf,sizeof(buf), "%d", value);
            config_setting_set_string(partitions, buf);
        }
        else
        {
            fprintf(stderr, "%s %d: partitions is of unknown type\n",
                __FILE__, __LINE__);
            goto error;
        }
    }

    return true;

    error:
    return false;
}

bool
kafka_producer_validator(config_setting_t *config)
{
    int int_val = 0;
    const char *val = NULL;
    bool res =  config_setting_lookup_bool(config,"transactional", &int_val);

    if (res == CONFIG_TRUE && int_val)
    {
        fprintf(stderr, "%s %d: Transactional producer detected\n"
            "Setting enable.idempotence=true\n", __FILE__, __LINE__);

        config_setting_t *rkopts
            = config_create_path(config, "kafka_options", CONFIG_TYPE_GROUP);
        config_set_default_string(rkopts, "enable_idempotence", "true");

        res = config_setting_lookup_string(config, "kafka_opts/transactional_id",
            &val);

        if(res)
        {
            fprintf(stderr, "%s %d: No support for transactional.id!\n",
                __FILE__, __LINE__);
            goto err;
        }
    }

    return kafka_validator(config);

    err:
    return false;
}

bool
kafka_consumer_validator(config_setting_t *config)
{
    const char *groupid = NULL;
    if (config_setting_lookup_string(config, "groupid", &groupid) != CONFIG_TRUE)
    {
        fprintf(stderr, "%s %d: Warning: No groupid found. Disabling"
            " high level consumer!\n", __FILE__, __LINE__);
        config_set_default_string(config, "kafka_options/enable_auto_commit",
            "false");
    }

    int value = 0;
    bool res = config_setting_lookup_bool(config, "transactional", &value);
    if (res == CONFIG_TRUE && value)
    {
        if(!groupid)
        {
            fprintf(stderr, "%s %d: transactional consumer but no group!\n",
                __FILE__, __LINE__);
            goto err;
        }

        fprintf(stderr, "%s %d: Transactional consumer detected. "
            "Setting enable.auto.offset.store=false\n", __FILE__, __LINE__);

        config_setting_t *rkopts
            = config_create_path(config, "kafka_options", CONFIG_TYPE_GROUP);
        config_set_default_string(rkopts, "enable_auto_offset_store", "false");
        // this should be user supplied
        // config_set_default_string(rkopts, "isolation_level", "read_committed");
    }

    return kafka_validator(config);
    err:

    return false;
}

Validator
kafka_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = &kafka_consumer_validator;
    v->validate_producer = &kafka_producer_validator;

    return(v);
}
