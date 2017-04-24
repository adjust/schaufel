#include <postgres.h>

typedef struct Meta {
    PGconn   *conn;
    PGresult *res;
    char     *conninfo;
    int       count;
    int       copy;
} *Meta;

char *
_connectinfo(char *hostname, int port)
{
	int len = strlen(hostname)
		    + number_length(port)
			+ strlen(" dbname=data user=postgres ")
			+ strlen(" host= ")
			+ strlen(" port= ");
    char *conninfo = calloc(len + 1, sizeof(*conninfo));
    snprintf(conninfo, len, "dbname=data user=postgres host=%s port=%d", hostname, port);
    return conninfo;
}

Meta
postgres_meta_init(char *hostname, int port)
{
    Meta m = calloc(1, sizeof(*m));
    m->conninfo = _connectinfo(hostname, port);
    m->conn = PQconnectdb(m->conninfo);
    if (PQstatus(m->conn) != CONNECTION_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn));
        abort();
    }
    return m;
}

void
postgres_meta_free(Meta *m)
{
    free((*m)->conninfo);
    PQfinish((*m)->conn);
    free(*m);
    *m = NULL;
}

Producer
postgres_producer_init(char *hostname, int port)
{
    Producer postgres = calloc(1, sizeof(*postgres));

    postgres->meta          = postgres_meta_init(hostname, port);
    postgres->producer_free = postgres_producer_free;
    postgres->produce       = postgres_producer_produce;

    return postgres;
}

void
postgres_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;

    if (m->copy == 0)
    {
        m->res = PQexec(m->conn, "COPY data FROM STDIN");
        if (PQresultStatus(m->res) != PGRES_COPY_IN)
        {
            logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn));
            abort();
        }
        m->copy = 1;
        PQclear(m->res);
    }
    char *buf = (char *) message_get_data(msg);
    char *newline = "\n";
    PQputCopyData(m->conn, buf, strlen(buf));
    PQputCopyData(m->conn, newline, 1);

    m->count = m->count + 1;
    if (m->count == 2000)
    {
        PQputCopyEnd(m->conn, NULL);
        m->count = 0;
        m->copy  = 0;
    }
}

void
postgres_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);
    if (m->copy != 0)
        PQputCopyEnd(m->conn, NULL);
    postgres_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
postgres_consumer_init(char *hostname, int port)
{
    Consumer postgres = calloc(1, sizeof(*postgres));

    postgres->meta          = postgres_meta_init(hostname, port);
    postgres->consumer_free = postgres_consumer_free;
    postgres->consume       = postgres_consumer_consume;

    return postgres;
}

int
postgres_consumer_consume(Consumer c, Message msg)
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
