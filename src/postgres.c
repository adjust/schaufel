#include <postgres.h>

typedef struct Meta {
    PGconn   *conn_master;
    PGconn   *conn_replica;
    PGresult *res;
    char     *conninfo;
    char     *conninfo_replica;
    char     *cpycmd;
    int       count;
    int       copy;
} *Meta;

char *
_connectinfo(char *host)
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
    char *conninfo = calloc(len + 1, sizeof(*conninfo));
    if (!conninfo) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }
    snprintf(conninfo, len, "dbname=data user=postgres host=%s port=%d", hostname, port);
    return conninfo;
}

static char *
_cpycmd(char *host, char *generation)
{
    if (host == NULL)
        return NULL;
    char *hostname;
    int port = 0;

    if (parse_connstring(host, &hostname, &port) == -1)
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

    char *cpycmd = calloc(len + 1, sizeof(*cpycmd));
    if (!cpycmd) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }
    snprintf(cpycmd, len, fmtstring, hostname, port, generation);
    return cpycmd;
}

Meta
postgres_meta_init(char *host, char *host_replica, char *nsp)
{
    Meta m = calloc(1, sizeof(*m));
    if (!m) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    m->cpycmd = _cpycmd(host, nsp);
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

    return m;
}

void
postgres_meta_free(Meta *m)
{
    free((*m)->conninfo);
    PQfinish((*m)->conn_master);
    if ((*m)->conninfo_replica == NULL)
        PQfinish((*m)->conn_replica);
    free((*m)->conninfo_replica);
    free(*m);
    *m = NULL;
}

Producer
postgres_producer_init(char *host, char *host_replica, char *nsp)
{
    Producer postgres = calloc(1, sizeof(*postgres));
    if (!postgres) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    postgres->meta          = postgres_meta_init(host, host_replica, nsp);
    postgres->producer_free = postgres_producer_free;
    postgres->produce       = postgres_producer_produce;

    return postgres;
}

void
postgres_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;

    char *buf = (char *) message_get_data(msg);
    char *newline = "\n";
    char *s = strstr(buf, "\\u0000");
    if (s != NULL)
    {
        logger_log("found invalid unicode byte sequence: %s", buf);
        return;
    }

    char *lit = PQescapeLiteral(m->conn_master, buf, strlen(buf));

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
        PQputCopyEnd(m->conn_master, NULL);
        if (m->conninfo_replica)
            PQputCopyEnd(m->conn_replica, NULL);
        m->count = 0;
        m->copy  = 0;
    }
    free(lit);
}

void
postgres_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);
    if (m->copy != 0)
    {
        PQputCopyEnd(m->conn_master, NULL);
        if (m->conninfo_replica)
            PQputCopyEnd(m->conn_replica, NULL);
    }
    postgres_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
postgres_consumer_init(char *host)
{
    Consumer postgres = calloc(1, sizeof(*postgres));
    if (!postgres) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    postgres->meta          = postgres_meta_init(host, NULL, NULL);
    postgres->consumer_free = postgres_consumer_free;
    postgres->consume       = postgres_consumer_consume;

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
