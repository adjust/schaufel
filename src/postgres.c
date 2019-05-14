#include <libpq-fe.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "modules.h"
#include "postgres.h"
#include "utils/array.h"
#include "utils/config.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/postgres.h"
#include "utils/scalloc.h"


char *
_connectinfo(const char *host)
{
    if (host == NULL)
        return NULL;
    char *hostname;
    int port = 0;

    if (parse_connstring(host, &hostname, &port) == -1)
        abort();

    int len = strlen(hostname)
            + number_length(port)
            + strlen(" dbname=data user=postgres ")
            + strlen(" host= ")
            + strlen(" port= ");
    char *conninfo = SCALLOC(len + 1, sizeof(*conninfo));

    snprintf(conninfo, len, "dbname=data user=postgres host=%s port=%d", hostname, port);
    free(hostname);
    return conninfo;
}

static char *
_cpycmd(const char *host, const char *generation)
{
    if (host == NULL)
        return NULL;
    char *hostname;
    int port = 0;

    if (parse_connstring((char *)host, &hostname, &port) == -1)
        abort();

    char *ptr = hostname;
    while(*ptr)
    {
        if(*ptr == '-')
        {
            *ptr = '_';
        }
        ++ptr;
    }

    const char *fmtstring = "COPY %s_%d_%s.data FROM STDIN";

    int len = strlen(hostname)
            + number_length(port)
            + strlen(fmtstring);

    char *cpycmd = SCALLOC(len + 1, sizeof(*cpycmd));

    snprintf(cpycmd, len, fmtstring, hostname, port, generation);
    free(hostname);
    return cpycmd;
}


Meta
postgres_meta_init(const char *host, const char *host_replica, const char *generation)
{
    Meta m = SCALLOC(1, sizeof(*m));

    m->cpycmd = _cpycmd(host, generation);
    m->conninfo = _connectinfo(host);

    m->conn_master = PQconnectdb(m->conninfo);
    if (PQstatus(m->conn_master) != CONNECTION_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn_master));
        abort();
    }

    m->conninfo_replica = _connectinfo(host_replica);

    if (m->conninfo_replica == NULL)
        return m;

    m->conn_replica = PQconnectdb(m->conninfo_replica);
    if (PQstatus(m->conn_replica) != CONNECTION_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn_replica));
        abort();
    }

    if (pthread_mutex_init(&m->commit_mutex, NULL) != 0) {
        logger_log("%s %d: unable to create mutex", __FILE__, __LINE__ );
        abort();
    }

    return m;
}

void
postgres_meta_free(Meta *m)
{
    pthread_mutex_destroy(&(*m)->commit_mutex);

    free((*m)->conninfo);
    PQfinish((*m)->conn_master);
    if ((*m)->conninfo_replica == NULL)
        PQfinish((*m)->conn_replica);
    free((*m)->conninfo_replica);
    free(*m);
    *m = NULL;
}

Producer
postgres_producer_init(config_setting_t *config)
{
    const char *host = NULL, *host_replica = NULL, *generation = NULL;

    config_setting_lookup_string(config,"host",&host);
    config_setting_lookup_string(config,"replica",&host_replica);
    config_setting_lookup_string(config,"topic", &generation);

    Producer postgres = SCALLOC(1, sizeof(*postgres));
    postgres->meta = postgres_meta_init(host, host_replica, generation);

    if (pthread_create(&((Meta)(postgres->meta))->commit_worker,
        NULL,
        commit_worker,
        (void *)&(postgres->meta))) {
        logger_log("%s %d: Failed to create commit worker!", __FILE__, __LINE__);
        abort();
    }

    return postgres;
}

void
postgres_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;

    char *buf = (char *) message_get_data(msg);
    size_t len = message_get_len(msg);
    char *newline = "\n";

    if (buf[len] != '\0')
    {
        logger_log("payload doesn't end on null terminator");
        return;
    }

    char *s = strstr(buf, "\\u0000");
    if (s != NULL)
    {
        logger_log("found invalid unicode byte sequence: %s", buf);
        return;
    }

    char *lit = PQescapeLiteral(m->conn_master, buf, strlen(buf));

    pthread_mutex_lock(&m->commit_mutex);
    if (m->copy == 0)
    {
        m->res = PQexec(m->conn_master, m->cpycmd);
        if (PQresultStatus(m->res) != PGRES_COPY_IN)
        {
            logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn_master));
            abort();
        }
        PQclear(m->res);

        if (m->conninfo_replica)
        {
            m->res = PQexec(m->conn_replica, m->cpycmd);
            if (PQresultStatus(m->res) != PGRES_COPY_IN)
            {
                logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn_replica));
                abort();
            }
            PQclear(m->res);
        }
        m->copy = 1;
    }

    if (lit[0] == ' ' && lit[1] == 'E')
    {
        PQputCopyData(m->conn_master, lit + 3, strlen(lit) - 4);
        if (m->conninfo_replica)
            PQputCopyData(m->conn_replica, lit + 3, strlen(lit) - 4);
    }
    else if (lit[0] == '\'')
    {
        PQputCopyData(m->conn_master, lit + 1, strlen(lit) - 2);
        if (m->conninfo_replica)
            PQputCopyData(m->conn_replica, lit + 1, strlen(lit) - 2);
    }
    else
        abort();

    PQputCopyData(m->conn_master, newline, 1);

    if (m->conninfo_replica)
        PQputCopyData(m->conn_replica, newline, 1);

    m->count = m->count + 1;
    if (m->count == 2000)
    {
        commit(&m);
    }
    pthread_mutex_unlock(&m->commit_mutex);

    free(lit);
}

void
postgres_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);

    pthread_cancel(m->commit_worker);
    void *res;
    pthread_join(m->commit_worker, &res);

    if (m->copy != 0)
    {
        commit(&m);
    }

    postgres_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
postgres_consumer_init(config_setting_t *config)
{
    Consumer postgres = SCALLOC(1, sizeof(*postgres));
    const char *host;

    config_setting_lookup_string(config, "host", &host);

    postgres->meta = postgres_meta_init(host, NULL, NULL);

    return postgres;
}

int
postgres_consumer_consume(UNUSED Consumer c, UNUSED Message msg)
{
    //TODO
    return -1;
}

void
postgres_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    postgres_meta_free(&m);
    free(*c);
    *c = NULL;
}

bool
postgres_validate(config_setting_t *config)
{
    config_setting_t *parent = NULL, *instance = NULL, *setting = NULL;
    const char *hosts = NULL, *replicas = NULL, *topic = NULL;

    Array master = NULL,replica = NULL;

    int m, r, threads;
    bool ret = true;

    // We need the parent list, because the postgres
    // consumer may add further consumers to the list.
    parent = config_setting_parent(config);

    if(!CONF_L_IS_STRING(config, "host", &hosts, "require host string!"))
        ret = false;
    if(!CONF_L_IS_INT(config, "threads", &threads, "require a threads integer"))
        ret = false;
    if(!ret) goto error;

    master = parse_hostinfo_master((char*) hosts);
    replica = parse_hostinfo_replica((char*) hosts);

    m = array_used(master);
    r = array_used(replica);

    if(config_setting_lookup_string(config, "replica", &replicas)
        == CONFIG_TRUE && r) {
        fprintf(stderr, "%s %d: replica %s conflicts with host list!\n",
            __FILE__, __LINE__, replicas);
        ret = false;
    }
    if(!CONF_L_IS_STRING(config, "topic", &topic, "need a topic/generation!"))
        ret = false;
    if(!ret) goto error;

    if(m == 0) {
        fprintf(stderr, "%s %d: I require at least one host!\n",
            __FILE__, __LINE__);
        ret = false;
        goto error;
    }
    if(r > m || ( r > 0 && r < m)) {
        fprintf(stderr, "%s %d (warning): master/replica count uneven!\n",
            __FILE__, __LINE__);
    }

    setting = config_setting_get_member(config, "host");
    config_setting_set_string(setting, array_get(master, 0));

    if(r) {
        setting = config_setting_add(config, "replica", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, array_get(replica, 0));
    }

    for (int i = 1; i < m; ++i)
    {
        instance = config_setting_add(parent, NULL, CONFIG_TYPE_GROUP);
        if(instance == NULL) {
            ret = false;
            goto  error;
        }
        setting = config_setting_add(instance, "type", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, "postgres");

        setting = config_setting_add(instance, "host", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, array_get(master, i));

        setting = config_setting_add(instance, "topic", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, topic);

        setting = config_setting_add(instance, "threads", CONFIG_TYPE_INT);
        config_setting_set_int(setting, threads);

        if(r && (array_get(replica,i) != NULL)) {
            setting = config_setting_add(instance, "replica", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, array_get(replica, i));
        }
    }

    error:
    array_free(&replica);
    array_free(&master);
    return ret;
}

Validator
postgres_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = postgres_validate;
    v->validate_producer = postgres_validate;
    return v;
}

void
register_postgres_module(void)
{
    ModuleHandler *handler = SCALLOC(1, sizeof(ModuleHandler));

    handler->consumer_init = postgres_consumer_init;
    handler->consume = postgres_consumer_consume;
    handler->consumer_free = postgres_consumer_free;
    handler->producer_init = postgres_producer_init;
    handler->produce = postgres_producer_produce;
    handler->producer_free = postgres_producer_free;
    handler->validator_init = postgres_validator_init;

    register_module("postgres", handler);
}

