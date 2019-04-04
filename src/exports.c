#include <exports.h>

typedef struct Needles *Needles;
typedef struct Needles {
    char*           jpointer;
    void*           to_binary;
    Needles         next;
    size_t          maxlength;
    size_t          length;
    char*           result;
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

static char *
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

    Needles needles = calloc(1,sizeof(*needles));
    if (!needles) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__,
        strerror(errno));
        abort();
    }
    Needles first = needles;

    do {
        printf("json_pointers: %s\n", *json_pointers);
        needles->jpointer = calloc(1,strlen(*json_pointers));
        strcpy(needles->jpointer, *json_pointers);

        // 2 kb ought to be a good start for a buffer
        needles->maxlength = 2048;
        needles->result = calloc(1, needles->maxlength);

        // TODO: turn flow into sanity
        json_pointers++;
        if (*json_pointers == NULL)
            break;

        Needles next = calloc(1,sizeof(*needles));
        if (!needles) {
            logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__,
            strerror(errno));
            abort();
        }
        needles->next = next;
        needles = needles->next;

    } while (*json_pointers != NULL);

    return(first);
}

static char *
_cpycmd(const char *host, const char *table)
{
    if (host == NULL)
        return NULL;
    char *hostname;
    int port = 0;

    if (parse_connstring(host, &hostname, &port) == -1)
        abort();

    const char *fmtstring = "COPY %s FROM STDIN ( FORMAT csv, QUOTE '\"')";

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

static void
_commit(Meta *m)
{
    PQputCopyEnd((*m)->conn_master, NULL);

    (*m)->count = 0;
    (*m)->copy  = 0;
    (*m)->commit_iter = 0;
}

static void
_commit_worker_cleanup(void *mutex)
{
    pthread_mutex_unlock((pthread_mutex_t*) mutex);
    return;
}

static void *
_commit_worker(void *meta)
{
    Meta *m = (Meta *) meta;

    #ifdef PR_SET_NAME
    prctl(PR_SET_NAME, "commit_worker");
    #endif


    while(42)
    {
        sleep(1);

        pthread_mutex_lock(&((*m)->commit_mutex));
        pthread_cleanup_push(_commit_worker_cleanup, &(*m)->commit_mutex);

        (*m)->commit_iter++;
        (*m)->commit_iter &= 0xF;

        /* if count > 0 it implies that copy == 1,
         * therefore it is safe to commit data */
        if((!((*m)->commit_iter)) && ((*m)->count > 0)) {
            logger_log("%s %d: Autocommiting %d entries",
                __FILE__, __LINE__, (*m)->count);
            _commit(m);
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&((*m)->commit_mutex));
        pthread_testcancel();
    }
}

Meta
exports_meta_init(const char *host, const char *topic)
{
    Meta m = calloc(1, sizeof(*m));
    if (!m) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    m->cpycmd = _cpycmd(host, topic);
    m->conninfo = _connectinfo(host);
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

    Needles last;

    while((*m)->needles != NULL) {
        last = (*m)->needles;
        free((*m)->needles->result);
        (*m)->needles = (*m)->needles->next;
        free(last);
    }

    free(*m);
    *m = NULL;
}

Producer
exports_producer_init(config_setting_t *config)
{
    const char *host = NULL, *topic = NULL;
    config_setting_lookup_string(config, "host", &host);
    config_setting_lookup_string(config, "topic", &topic);
    Producer exports = calloc(1, sizeof(*exports));
    if (!exports) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    exports->meta          = exports_meta_init(host, topic);
    exports->producer_free = exports_producer_free;
    exports->produce       = exports_producer_produce;

    if (pthread_create(&((Meta)(exports->meta))->commit_worker,
        NULL,
        _commit_worker,
        (void *)&(exports->meta))) {
        logger_log("%s %d: Failed to create commit worker!", __FILE__, __LINE__);
        abort();
    }

    return exports;
}

int
_deref(char* data, Needles needles)
{
    char* jstring;
    struct json_object* haystack;
    struct json_object* needle;

    haystack = json_tokener_parse(data);
    if(!haystack)
        return -1;

    while(needles) {
        if(json_pointer_get(haystack, needles->jpointer, &needle)) {
            strlcpy(needles->result, "\0", 1);
            needles->length = 0;
        } else {
            jstring = (char*) json_object_to_json_string_length(
                needle, JSON_C_TO_STRING_NOSLASHESCAPE, &(needles->length));

            // grow buffer if needed
            if(needles->length >= needles->maxlength) {
                needles->result = realloc(needles->result, needles->length+1);
                if(!needles->result) {
                    logger_log("%s %d: Failed to realloc!", __FILE__, __LINE__);
                    abort();
                }
                needles->maxlength = needles->length+1;
                logger_log("%s %d: Reallocating result buffer"
                    " (consider increasing)!",
                    __FILE__, __LINE__);
            }
            strlcpy(needles->result, jstring, (needles->length+1));
        }
        needles = needles->next;
    }

    json_object_put(haystack);
    return 0;
}

void
exports_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;
    Needles needles = m->needles;

    size_t len = message_get_len(msg);
    char* data = message_get_data(msg);
    // Make buffer larger than message
    char *buf = calloc(2,len);
    char *newline = "\n";

    if (data[len] != '\0')
    {
        logger_log("payload doesn't end on null terminator");
        return;
    }

    if(_deref(data, needles)) {
        logger_log("%s %d: Failed to tokenize json!", __FILE__, __LINE__);
        return;
    }
    needles = m->needles;

    while(needles) {
        strcat(buf, needles->result);

        needles = needles->next;
        if (!needles)
            break;
        strcat(buf, ",");
    }

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

    PQputCopyData(m->conn_master, buf, strlen(buf));
    PQputCopyData(m->conn_master, newline, 1);

    m->count = m->count + 1;
    if (m->count == 2000)
    {
        _commit(&m);
    }
    pthread_mutex_unlock(&m->commit_mutex);

    free(buf);
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
        _commit(&m);
    }

    exports_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
exports_consumer_init(config_setting_t *config)
{
    const char *host = NULL;
    config_setting_lookup_string(config, "host", &host);
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

bool
exporter_validate(UNUSED config_setting_t *config)
{
    return true;
}


Validator
exports_validator_init()
{
    Validator v = calloc(1,sizeof(*v));
    v->validate_consumer = &exporter_validate;
    v->validate_producer = &exporter_validate;
    return v;
}
