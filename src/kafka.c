#include <kafka.h>
#include <errno.h>

static void
dr_msg_cb (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
    if (rkmessage->err)
        logger_log("%s %d: Message delivery failed: %s\n", __FILE__, __LINE__, rd_kafka_err2str(rkmessage->err));
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
rebalance_cb (rd_kafka_t *rk, rd_kafka_resp_err_t err, rd_kafka_topic_partition_list_t *partitions, void *opaque)
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

typedef struct Meta {
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    rd_kafka_conf_t *conf;
    rd_kafka_topic_partition_list_t *topics;
} *Meta;

Meta
kafka_producer_meta_init(char *broker, char *topic)
{
    Meta m = calloc(1, sizeof(*m));
    char errstr[512];
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    rd_kafka_conf_t *conf;

    conf = rd_kafka_conf_new();

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
kafka_consumer_meta_init(char *broker, char *topic, char *groupid)
{
    Meta m = calloc(1, sizeof(*m));
    rd_kafka_resp_err_t err;
    char errstr[512];
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    rd_kafka_conf_t *conf;
    rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();

    conf = rd_kafka_conf_new();

    if (rd_kafka_conf_set(conf, "group.id", groupid, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, errstr);
        abort();
    }

    if (rd_kafka_topic_conf_set(topic_conf, "offset.store.method","broker",errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s\n", __FILE__, __LINE__, errstr);
        abort();
    }
    if (rd_kafka_topic_conf_set(topic_conf, "enable.auto.commit","true",errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s\n", __FILE__, __LINE__, errstr);
        abort();
    }
    if (rd_kafka_topic_conf_set(topic_conf, "auto.commit.interval.ms","10",errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s\n", __FILE__, __LINE__, errstr);
        abort();
    }

    if (rd_kafka_topic_conf_set(topic_conf, "auto.offset.reset","beginning",errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        logger_log("%s %d: %s\n", __FILE__, __LINE__, errstr);
        abort();
    }

    rd_kafka_conf_set_default_topic_conf(conf, topic_conf);
    rd_kafka_conf_set_rebalance_cb(conf, rebalance_cb);
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        logger_log("%s %d: Failed to create new producer: %s\n", __FILE__, __LINE__, errstr);
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
    rd_kafka_topic_partition_list_add(topics, "test", -1);

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
kafka_producer_init(char *broker, char *topic)
{
    Producer kafka = calloc(1, sizeof(*kafka));

    kafka->meta          = kafka_producer_meta_init(broker, topic);
    kafka->producer_free = kafka_producer_free;
    kafka->produce       = kafka_producer_produce;

    return kafka;
}

void
kafka_producer_produce(Producer p, Message msg)
{
    char *buf = (char *) message_get_data(msg);
    size_t len = strlen(buf);
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
kafka_consumer_init(char *broker, char *topic, char *groupid)
{
    Consumer kafka = calloc(1, sizeof(*kafka));

    kafka->meta          = kafka_consumer_meta_init(broker, topic, groupid);
    kafka->consumer_free = kafka_consumer_free;
    kafka->consume       = kafka_consumer_consume;

    return kafka;
}

int
kafka_consumer_consume(Consumer c, Message msg)
{

    rd_kafka_t *rk = ((Meta) c->meta)->rk;
    rd_kafka_message_t *rkmessage;

    rkmessage = rd_kafka_consumer_poll(rk, 10000);
    if (rkmessage)
    {

        if (rkmessage->err)
        {
            if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)
                logger_log("Consumer reached end of %s [%"PRId32"] "
                    "message queue at offset %"PRId64"\n",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition, rkmessage->offset);

            if (rkmessage->rkt)
            {
                logger_log("%% Consume error for "
                    "topic \"%s\" [%"PRId32"] "
                    "offset %"PRId64": %s\n",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rd_kafka_message_errstr(rkmessage));
                rd_kafka_message_destroy(rkmessage);
                return 0;
            }
            else
                logger_log("%% Consumer error: %s: %s\n",
                    rd_kafka_err2str(rkmessage->err),
                    rd_kafka_message_errstr(rkmessage));

            if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION ||
                rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
                abort();
        }

        char *cpy = calloc((int)rkmessage->len + 1, sizeof(*cpy));
        strncpy(cpy, (char *)rkmessage->payload, (int)rkmessage->len);
        message_set_data(msg, cpy);
        rd_kafka_message_destroy(rkmessage);
    }
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
