#include <json-exports.h>

typedef struct Needles {
    char**          pointers;
    size_t          elements;
} *Needles;

typedef struct Meta {
    PGconn          *conn_master;
    PGresult        *res;
    char            *conninfo;
    char            *cpycmd;
    int             count;
    int             copy;
    int             commit_iter;
    pthread_mutex_t commit_mutex;
    pthread_t       commit_worker;
    Needles         needles;
} *Meta;

char *
_connectinfo2(char *host)
{
    if (host == NULL)
        return NULL;
    char *hostname;
    int port = 0;

    if (parse_connstring(host, &hostname, &port) == -1)
        abort();

    int len = strlen(hostname)
            + number_length(port)
            + strlen(" dbname=bagger_exports user=postgres ")
            + strlen(" host= ")
            + strlen(" port= ");
    char *conninfo = calloc(len + 1, sizeof(*conninfo));
    if (!conninfo) {
        logger_log("%s %d: Failed to calloc: %s\n",
            __FILE__, __LINE__, strerror(errno));
        abort();
    }
    snprintf(conninfo, len, "dbname=bagger_exports"
        " user=postgres host=%s port=%d", hostname, port);
    return conninfo;
}

Needles
_needles()
{
    /* replace with dynamic parser */
    const char** json_pointers = (const char *[]) {
        "/context/app_token",
        "/context/tracker_token",
        "/context/device_ids/idfa",
        "/context/device_ids/gps_adid",
        "/context/device_ids/imei",
        "/context/request/Header/User-Agent/0",
        "/context/impression/OsName",
        "/context/impression/IpAddress",
        "/context/impression/ServerIp",
        "/context/impression/CreatedAt",
        "/context/impression/OsVersion",
        "/context/impression/ReferenceTag",
        "/timestamp",
        NULL
    };
    size_t i;

    Needles needles = calloc(1,sizeof(*needles));
    if (!needles) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__,
        strerror(errno));
        abort();
    }
    needles->elements = 0;
    do {
        if(*(json_pointers+(needles->elements)) == NULL)
            break;
        needles->elements++;
    } while (1);

    printf("elements %ld\n", needles->elements);
    needles->pointers = calloc(needles->elements+1,sizeof(char*));

    for (i = 0; i < needles->elements; i++) {
        if (*json_pointers == NULL)
            break;
        *((needles->pointers)+i) = malloc(strlen(*json_pointers));
        strcpy(*((needles->pointers)+i), *json_pointers);
        printf("%ld %s\n", i, *((needles->pointers)+i));
        json_pointers++;
    };

    return(needles);
}

static char *
_cpycmd(char *host, char *table)
{
    if (host == NULL)
        return NULL;
    char *hostname;
    int port = 0;

    if (parse_connstring(host, &hostname, &port) == -1)
        abort();

    const char *fmtstring = "COPY %s FROM STDIN";

    int len = strlen(table)
            + strlen(fmtstring);

    char *cpycmd = calloc(len + 1, sizeof(*cpycmd));
    if (!cpycmd) {
        logger_log("%s %d: Failed to calloc: %s\n",
        __FILE__, __LINE__, strerror(errno));
        abort();
    }
    snprintf(cpycmd, len, fmtstring, table);
    return cpycmd;
}

void
_commit2(Meta *m)
{
    PQputCopyEnd((*m)->conn_master, NULL);

    (*m)->count = 0;
    (*m)->copy  = 0;
    (*m)->commit_iter = 0;
}

void
_commit2_worker2_cleanup(void *mutex)
{
    pthread_mutex_unlock((pthread_mutex_t*) mutex);
    return;
}

void *
_commit2_worker2(void *meta)
{
    Meta *m = (Meta *) meta;

    #ifdef PR_SET_NAME
    prctl(PR_SET_NAME, "commit_worker");
    #endif


    while(42)
    {
        sleep(1);

        pthread_mutex_lock(&((*m)->commit_mutex));
        pthread_cleanup_push(_commit2_worker2_cleanup, &(*m)->commit_mutex);

        (*m)->commit_iter++;
        (*m)->commit_iter &= 0xF;

        /* if count > 0 it implies that copy == 1,
         * therefore it is safe to commit data */
        if((!((*m)->commit_iter)) && ((*m)->count > 0)) {
            logger_log("%s %d: Autocommiting %d entries",
                __FILE__, __LINE__, (*m)->count);
            _commit2(m);
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&((*m)->commit_mutex));
        pthread_testcancel();
    }

}

Meta
exports_meta_init(char *host, char *nsp)
{
    Meta m = calloc(1, sizeof(*m));
    if (!m) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    m->cpycmd = _cpycmd(host, nsp);
    m->conninfo = _connectinfo2(host);
    m->needles = _needles();

    m->conn_master = PQconnectdb(m->conninfo);
    if (PQstatus(m->conn_master) != CONNECTION_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn_master));
        abort();
    }

    if (pthread_mutex_init(&m->commit_mutex, NULL) != 0) {
        logger_log("%s %d: unable to create mutex", __FILE__, __LINE__ );
        abort();
    }

    return m;
}

void
exports_meta_free(Meta *m)
{
    pthread_mutex_destroy(&(*m)->commit_mutex);

    free((*m)->conninfo);
    PQfinish((*m)->conn_master);

    size_t i = 0;
    for (i = 0; i < (*m)->needles->elements; i++) {
        free(*((*m)->needles->pointers+i));
    }
    free((*m)->needles->pointers);
    free((*m)->needles);

    free(*m);
    *m = NULL;
}

Producer
exports_producer_init(char *host, char *nsp)
{
    Producer exports = calloc(1, sizeof(*exports));
    if (!exports) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    exports->meta          = exports_meta_init(host, nsp);
    exports->producer_free = exports_producer_free;
    exports->produce       = exports_producer_produce;

    if (pthread_create(&((Meta)(exports->meta))->commit_worker,
        NULL,
        _commit2_worker2,
        (void *)&(exports->meta))) {
        logger_log("%s %d: Failed to create commit worker!", __FILE__, __LINE__);
        abort();
    }

    return exports;
}

int
_deref(Message msg, Needles needles, char** result)
{
    size_t length;
    char* jstring;
    struct json_object* haystack;
    struct json_object* needle;
    size_t i = 0;
    haystack = json_tokener_parse(message_get_data(msg));
    if(!haystack)
        return -1;

    for (i = 0; i < needles->elements; i++)
    {
        if(json_pointer_get(haystack, *(needles->pointers+i), &needle))
        {
            // Assume value does not exit, put null in array
            *(result+i) = calloc(1,sizeof(char));
        }
        else {
            jstring = (char*) json_object_to_json_string_length(needle,
                0, &length);
            *(result+i) = malloc(length+2);
            strlcpy(*(result+i), jstring, length+1);
        }
    };

    json_object_put(haystack);
    return 0;
}

void
exports_producer_produce(Producer p, Message msg)
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

    /* This code should be unneeded with json-c
    char *s = strstr(buf, "\\u0000");
    if (s != NULL)
    {
        logger_log("found invalid unicode byte sequence: %s", buf);
        return;
    }
    */
    size_t i;

    char **result = calloc(m->needles->elements, sizeof(char*));
    if(_deref(msg, m->needles, result)) {
        free(result);
        puts("failed to parse json");
        return;
    }
    for (i = 0; i < m->needles->elements; i++)
    {
        printf("result: %s\n", *(result+i));
        free(*(result+i));
    }
    free(result);
    return;

    char *lit = PQescapeLiteral(m->conn_master, buf, strlen(buf));

    pthread_mutex_lock(&m->commit_mutex);
    if (m->copy == 0)
    {
        m->res = PQexec(m->conn_master, m->cpycmd);
        if (PQresultStatus(m->res) != PGRES_COPY_IN)
        {
            logger_log("%s %d: %s", __FILE__, __LINE__,
                PQerrorMessage(m->conn_master));
            abort();
        }
        PQclear(m->res);

        m->copy = 1;
    }

    if (lit[0] == ' ' && lit[1] == 'E')
    {
        PQputCopyData(m->conn_master, lit + 3, strlen(lit) - 4);
    }
    else if (lit[0] == '\'')
    {
        PQputCopyData(m->conn_master, lit + 1, strlen(lit) - 2);
    }
    else
        abort();

    PQputCopyData(m->conn_master, newline, 1);

    m->count = m->count + 1;
    if (m->count == 2000)
    {
        _commit2(&m);
    }
    pthread_mutex_unlock(&m->commit_mutex);

    free(lit);
}

void
exports_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);

    pthread_cancel(m->commit_worker);
    void *res;
    pthread_join(m->commit_worker, &res);

    if (m->copy != 0)
    {
        _commit2(&m);
    }

    exports_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
exports_consumer_init(char *host)
{
    Consumer exports = calloc(1, sizeof(*exports));
    if (!exports) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    exports->meta          = exports_meta_init(host, NULL);
    exports->consumer_free = exports_consumer_free;
    exports->consume       = exports_consumer_consume;

    return exports;
}

int
exports_consumer_consume(UNUSED Consumer c, UNUSED Message msg)
{
    //TODO
    return -1;
}

void
exports_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    exports_meta_free(&m);
    free(*c);
    *c = NULL;
}
