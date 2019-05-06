#include <exports.h>
#include <utils/postgres.h>

typedef struct Needles *Needles;
typedef struct Needles {
    char           *jpointer;
    bool          (*format) (json_object *, Needles);
    void          (*free) (void **);
    Needles         next;
    uint32_t        length;
    void           *result;
    uint32_t       *leapyear;
} *Needles;

typedef struct Internal {
    Needles         needles;
    uint16_t        ncount;
} *Internal;

static bool _json_to_pqtext (json_object *needle, Needles current);
static bool _json_to_pqtimestamp (json_object *needle, Needles current);
static void _obj_noop(UNUSED void **obj);
static void _obj_free(void **obj);

typedef enum {
    undef,
    text,
    timestamp,
} PqTypes;

static const struct {
    PqTypes type;
    const char *pq_type;
    bool  (*format) (json_object *, Needles);
    void  (*free) (void **obj);
}  pq_types [] = {
        {undef, "undef", NULL, NULL},
        {text, "text", &_json_to_pqtext, &_obj_noop},
        {timestamp, "timestamp", &_json_to_pqtimestamp, &_obj_free},
};

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
    char *conninfo = SCALLOC(len + 1, sizeof(*conninfo));

    snprintf(conninfo, len, "dbname=bagger_exports"
        " user=postgres host=%s port=%d", hostname, port);
    free(hostname);
    return conninfo;
}

static PqTypes
_pqtype_enum(const char *pqtype)
{
    uint32_t j;
    for (j = 1; j < sizeof(pq_types) / sizeof(pq_types[0]); j++)
        if (!strcmp(pqtype, pq_types[j].pq_type))
            return pq_types[j].type;
    return undef;
}

static uint32_t *
_leapyear()
{
    // 2048 years ought to be enough
    uint32_t *l = SCALLOC(2048,sizeof(uint32_t));
    uint32_t a = 0;

    for (uint32_t i = 0; i < 2047; i++) {
        if ((i % 4 == 0 && i % 100 != 0) || (i % 400 == 0)) {
            a += 1;
        }
        //The array starts with 0 accumulated days
        *(l+i+1) = a;
    }

    return l;
}


static void _obj_noop(UNUSED void **obj)
{
    //noop; object is managed by json-c
    return;
}

static void _obj_free(void **obj)
{
    // this function is supposed to be idempotent
    if(!obj)
        return;
    if(!*obj)
        return;
    free(*obj);
    *obj = NULL;
}

static bool
_json_to_pqtext(json_object *needle, Needles current)
{
    // Any json type can be cast to string
    current->result = (char *)json_object_get_string(needle);
    current->length = strlen(current->result);
    return true;
}

static bool
_json_to_pqtimestamp(json_object *needle, Needles current)
{
    const char *ts = json_object_get_string(needle);
    uint64_t epoch;
    size_t len = strlen(ts);

    struct {
        uint32_t year;
        uint32_t month;
        uint32_t day;
        uint32_t yday;
        uint32_t hour;
        uint32_t minute;
        uint32_t second;
        uint64_t micro;
    } tm;

    memset(&tm,0,sizeof(tm));

    // todo: static memory allocation in internal?
    current->result = malloc(sizeof(uint64_t));
    if(!current->result)
        goto error;

    if (len < 21 || len > 31) {
        logger_log("%s %d: Datestring %s not supported",
            __FILE__, __LINE__, ts);
        goto error;
    }

    // If a timestamp is not like 2000-01-01T00:00:01.000000Z
    // it's considered invalid. Z stands for Zulu/UTC
    if (ts[4] != '-' || ts[7] != '-' || ts[10] != 'T'
            || ts[13] != ':' || ts[16] != ':' || ts[19] != '.'
            || ts[len-1] != 'Z') {
        logger_log("%s %d: Datestring %s not supported",
            __FILE__, __LINE__, ts);
        goto error;
    }

    errno = 0;
    tm.year = strtoul(ts,NULL,10);
    tm.month = strtoul(ts+5,NULL,10);
    tm.day = strtoul(ts+8,NULL,10);
    tm.hour = strtoul(ts+11,NULL,10);
    tm.minute = strtoul(ts+14,NULL,10);
    tm.second = strtoul(ts+17,NULL,10);
    if(ts[20] != 'Z')
        tm.micro = strtoull(ts+20,NULL,10);

    if(errno) {
        logger_log("%s %d: Error %s in date conversion",
            __FILE__, __LINE__, strerror(errno));
        goto error;
    }

    if(len-21 > 6) {
        for (uint8_t i = 0; i < (len-21-6); i++)
            tm.micro /= 10;
    } else if(len-21 < 6) {
        for (uint8_t i = 0; i < (6-(len-21)); i++)
            tm.micro *= 10;
    }

    if(tm.year < 2000 || tm.year > 4027) {
        logger_log("%s %d: Date %s out of range",
            __FILE__, __LINE__, ts);
        goto error;
    }
    if(tm.month < 1 || tm.month > 12 || tm.day >31
        || tm.hour > 23 || tm.minute > 59 || tm.second > 59) {
        logger_log("%s %d: Datestring %s not a date",
            __FILE__, __LINE__, ts);
        goto error;
    }
    if(tm.month == 2 && tm.day > 29) {
        logger_log("%s %d: Datestring %s not a date",
            __FILE__, __LINE__, ts);
        goto error;
    }

    // postgres starts at 2000-01-01
    tm.year -= 2000;

    // day of year
    for (uint8_t i = 1; i < tm.month; i++) {
        if(i == 2) {
            if ((tm.year % 4 == 0 && tm.year % 100 != 0) || (tm.year % 400 == 0))
                tm.yday += 29;
            else
                tm.yday += 28;
        } else {
            tm.yday += 30 + (i%2);
        }
    }

    tm.yday += tm.day;

    epoch = tm.second + tm.minute*60 + tm.hour*3600 + (tm.yday-1)*86400
    + *(current->leapyear+(tm.year))*86400 + tm.year*31536000;

    epoch *= 1000000;
    epoch += tm.micro;

    *((uint64_t *) current->result) = htobe64(epoch);
    current->length = sizeof(uint64_t);

    return true;
    error:
    return false;
}

static Needles
_needles(config_setting_t *needlestack)
{
    config_setting_t *setting = NULL, *member= NULL;
    int list = 0, pqtype = 0;
    list = config_setting_length(needlestack);

    Needles first = SCALLOC(1,sizeof(*first));
    Needles needle = first;
    for(int i = 0; i < list; i++) {
        setting = config_setting_get_elem(needlestack, i);

        if (config_setting_type(setting) == CONFIG_TYPE_STRING) {
            pqtype = _pqtype_enum("text");
            needle->jpointer = strdup(config_setting_get_string(setting));
            needle->format = pq_types[pqtype].format;
            needle->free = pq_types[pqtype].free;
        } else if (config_setting_type(setting) == CONFIG_TYPE_ARRAY) {
            member = config_setting_get_elem(setting, 0);
            needle->jpointer = strdup(config_setting_get_string(member));
            member = config_setting_get_elem(setting, 1);
            pqtype = _pqtype_enum(config_setting_get_string(member));
            needle->format = pq_types[pqtype].format;
            needle->free = pq_types[pqtype].free;
        }

        // Leapyears in every needle to save on infrastructure
        needle->leapyear = _leapyear();

        needle->next = SCALLOC(1,sizeof(*needle));
        needle = needle->next;
    }

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

    const char *fmtstring = "COPY %s FROM STDIN ( FORMAT binary )";

    int len = strlen(table)
            + strlen(fmtstring);

    char *cpycmd = SCALLOC(len + 1, sizeof(*cpycmd));

    snprintf(cpycmd, len, fmtstring, table);
    free(hostname);
    return cpycmd;
}


Meta
exports_meta_init(const char *host, const char *topic, config_setting_t *needlestack)
{
    Meta m = SCALLOC(1, sizeof(*m));
    Internal i = SCALLOC(1,sizeof(*i));

    m->cpyfmt = PQ_COPY_BINARY;
    m->internal = i;
    m->cpycmd = _cpycmd(host, topic);
    m->conninfo = _connectinfo(host);
    m->internal->needles = _needles(needlestack);
    m->internal->ncount = config_setting_length(needlestack);

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
    free((*m)->cpycmd);
    PQfinish((*m)->conn_master);

    Needles last;

    while((*m)->internal->needles != NULL) {
        last = (*m)->internal->needles;
        if(last->jpointer == NULL) {
            free(last);
            break;
        }
        free((*m)->internal->needles->jpointer);
        (*m)->internal->needles->free(
            &(*m)->internal->needles->result);
        if((*m)->internal->needles->leapyear != NULL)
            free((*m)->internal->needles->leapyear);
        (*m)->internal->needles = (*m)->internal->needles->next;
        free(last);
    }
    free((*m)->internal);

    free(*m);
    *m = NULL;
}

Producer
exports_producer_init(config_setting_t *config)
{
    const char *host = NULL, *topic = NULL;
    config_setting_t *needlestack = NULL;
    config_setting_lookup_string(config, "host", &host);
    config_setting_lookup_string(config, "topic", &topic);
    needlestack = config_setting_get_member(config, "jpointers");
    Producer exports = SCALLOC(1, sizeof(*exports));

    exports->meta          = exports_meta_init(host, topic, needlestack);
    exports->producer_free = exports_producer_free;
    exports->produce       = exports_producer_produce;

    if (pthread_create(&((Meta)(exports->meta))->commit_worker,
        NULL,
        commit_worker,
        (void *)&(exports->meta))) {
        logger_log("%s %d: Failed to create commit worker!", __FILE__, __LINE__);
        abort();
    }

    return exports;
}

bool
_deref(json_object *haystack, Needles needles)
{
    json_object *needle;

    while(needles->next) {
        if(json_pointer_get(haystack, needles->jpointer, &needle)) {
            needles->result = NULL;
            // complement of 0 is uint32_t max
            // int max is postgres NULL;
            needles->length = (uint32_t)~0;
        } else {
            if(!needles->format(needle,needles)) {
                logger_log("%s %d: Failed jpointer deref %s",
                    __FILE__, __LINE__, needles->jpointer);
                return false;
            }
        }
        needles = needles->next;
    }

    return true;
}

void
exports_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;
    Needles needles = m->internal->needles;
    json_object *haystack = NULL;

    size_t len = message_get_len(msg);
    char* data = message_get_data(msg);

    uint16_t ncount = htons(m->internal->ncount);

    if (data[len] != '\0')
    {
        logger_log("payload doesn't end on null terminator");
        return;
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
        PQputCopyData(m->conn_master,
            "PGCOPY\n\377\r\n\0" //postgres magic
            "\0\0\0\0"  // flags field (only bit 16 relevant)
            "\0\0\0\0"  // Header extension area
            , 19);
        m->copy = 1;
    }

    haystack = json_tokener_parse(data);
    if(!haystack) {
        logger_log("%s %d: Failed to tokenize json!", __FILE__, __LINE__);
        goto error;
    }

    if(!_deref(haystack, needles)) {
        logger_log("%s %d: Failed to dereference json!\n %s",
            __FILE__, __LINE__, data);
        while(needles->next) {
            if(!needles->jpointer)
                break;
            needles->free(&needles->result);
            needles = needles->next;
        }
        goto error;
    }

    needles = m->internal->needles;
    PQputCopyData(m->conn_master, (char *) &ncount, 2);

    while(needles->next) {
        if(!needles->jpointer)
            break;

        uint32_t length =  htobe32(needles->length);
        PQputCopyData(m->conn_master, (void *) &length, 4);

        if(needles->result) {
            PQputCopyData(m->conn_master, needles->result, needles->length);
            needles->free(&needles->result);
        }

        needles = needles->next;
    }

    m->count = m->count + 1;
    if (m->count == 2000)
    {
        commit(&m);
    }

    error:
    json_object_put(haystack);
    pthread_mutex_unlock(&m->commit_mutex);
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
        commit(&m);
    }

    exports_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
exports_consumer_init(config_setting_t *config)
{
    const char *host = NULL;
    config_setting_t *needles = NULL;
    config_setting_lookup_string(config, "host", &host);
    Consumer exports = SCALLOC(1, sizeof(*exports));

    exports->meta          = exports_meta_init(host, NULL, needles);
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
    config_setting_t *setting = NULL, *member = NULL, *child = NULL;
    const char *conf = NULL;

    const char *host = NULL, *topic = NULL;
    if(config_setting_lookup_string(config, "host", &host) != CONFIG_TRUE) {
        fprintf(stderr, "%s %d: require host string!\n",
            __FILE__, __LINE__);
        goto error;
    }

    if(config_setting_lookup_string(config, "topic", &topic) != CONFIG_TRUE) {
        fprintf(stderr, "%s %d: require topic string!\n",
            __FILE__, __LINE__);
        goto error;
    }
    if(!(setting = config_setting_get_member(config,"jpointers"))) {
        fprintf(stderr, "%s %d: require jpointers!\n",
            __FILE__, __LINE__);
        goto error;
    }
    if (config_setting_is_list(setting) != CONFIG_TRUE) {
        fprintf(stderr, "%s %d: require jpointer must be a list\n",
            __FILE__, __LINE__);
        goto error;
    }

    // Check jpointers for validity
    for (int i = 0; i < config_setting_length(setting); i++) {
        child = config_setting_get_elem(setting, i);
        if (child == NULL) {
            fprintf(stderr, "%s %d: failed to read jpointer list elem %d\n",
            __FILE__, __LINE__, i);
            goto error;
        }

        if (config_setting_is_array(child) == CONFIG_TRUE) {
            member = config_setting_get_elem(child, 1);
            if(member == NULL) {
                fprintf(stderr, "%s %d: failed to read jpointer array %d\n",
                __FILE__, __LINE__, i);
                goto error;
            }

            conf = config_setting_get_string(member);
            if(conf == NULL || !_pqtype_enum(conf) ) {
                fprintf(stderr, "%s %d: not a valid type transformation %s",
                __FILE__, __LINE__, conf);
                goto error;
            }
        } else if (config_setting_is_scalar(child) != CONFIG_TRUE)  {
            fprintf(stderr, "%s %d: jpointer needs to be a string/array\n",
            __FILE__, __LINE__);
        }
    }

    return true;
    error:
    return false;
}


Validator
exports_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = &exporter_validate;
    v->validate_producer = &exporter_validate;
    return v;
}
