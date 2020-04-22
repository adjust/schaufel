#include <errno.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "exports.h"
#include "utils/config.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/postgres.h"
#include "utils/scalloc.h"
#include "utils/endian.h"


static void exports_producer_free(Producer *p);
static void exports_producer_produce(Producer p, Message msg);
static void exports_consumer_free(Consumer *c);
static int exports_consumer_consume(Consumer c, Message msg);

static void _obj_noop(UNUSED void **obj);
static void _obj_free(void **obj);

static bool _filter_match (bool jpointer, json_object *found, Needles current);
static bool _filter_noop (bool jpointer, json_object *found, Needles current);
static bool _filter_substr (bool jpointer, json_object *found, Needles current);
static bool _filter_exists (bool jpointer, json_object *found, Needles current);

static bool _action_store (bool filter_ret, json_object *found, Needles current);
static bool _action_store_true (bool filter_ret, json_object *found, Needles current);
static bool _action_discard_false (bool filter_ret, json_object *found, Needles current);
static bool _action_discard_true (bool filter_ret, json_object *found, Needles current);

static bool _json_to_pqtext(json_object *found, Needles current);
static bool _json_to_pqtimestamp(json_object *found, Needles current);


typedef struct {
    const char *action_type;
    bool  (*action) (bool, json_object *, Needles);
    bool store;
} ExportsAction;

typedef struct {
    const char *filter_type;
    bool (*filter) (bool, json_object *, Needles);
    bool needs_data;
} ExportsFilter;

typedef struct {
    const char *pq_type;
    bool  (*format) (json_object *, Needles);
    void  (*free) (void **obj);
} PqType;

static const ExportsAction action_types[] = {
        {"undef", NULL, false},
        {"store", &_action_store, true},
        {"store_true", &_action_store_true, true},
        {"discard_false", &_action_discard_false, false},
        {"discard_true", &_action_discard_true, false}
};

static const ExportsFilter filter_types[] = {
        {"undef", NULL, false},
        {"noop", &_filter_noop, false},
        {"match", &_filter_match, true},
        {"substr", &_filter_substr, true},
        {"exists", &_filter_exists, false},
// TODO:        {"pcrematch", NULL, true},
};

static const PqType pq_types[] = {
        {"undef", NULL, NULL},
        {"text", &_json_to_pqtext, &_obj_noop},
        {"timestamp", &_json_to_pqtimestamp, &_obj_free},
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

static const ExportsAction *
lookup_actiontype(const char *action_type)
{
    uint32_t j;
    for (j = 1; j < sizeof(action_types) / sizeof(action_types[0]); j++)
        if (!strcmp(action_type, action_types[j].action_type))
            return &action_types[j];
    return NULL;
}


static const ExportsFilter *
lookup_filtertype(const char *filter_type)
{
    uint32_t j;
    for (j = 1; j < sizeof(filter_types) / sizeof(filter_types[0]); j++)
        if (!strcmp(filter_type, filter_types[j].filter_type))
            return &filter_types[j];
    return NULL;
}

static const PqType *
lookup_pqtype(const char *pqtype)
{
    uint32_t j;
    for (j = 1; j < sizeof(pq_types) / sizeof(pq_types[0]); j++)
        if (!strcmp(pqtype, pq_types[j].pq_type))
            return &pq_types[j];
    return NULL;
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
_action_store(UNUSED bool filter_ret, UNUSED json_object *found,
    UNUSED Needles current)
{
    return true;
}

static bool
_action_store_true(bool filter_ret, UNUSED json_object *found,
    UNUSED Needles current)
{
    return filter_ret;
}

static bool
_action_discard_false(bool filter_ret, UNUSED json_object *found,
    UNUSED Needles current)
{
    return filter_ret;
}

static bool
_action_discard_true(bool filter_ret, UNUSED json_object *found,
    UNUSED Needles current)
{
    return !filter_ret;
}

static bool
_filter_match(bool jpointer, json_object *found,
    UNUSED Needles current)
{
    if(jpointer == false || !found) // no data to match against
        return false;
    if(strcmp(json_object_get_string(found), current->filter_data) == 0)
        return true;
    return false;
}

static bool
_filter_substr(bool jpointer, json_object *found,
    UNUSED Needles current)
{
    if(jpointer == false || !found) // no data to match against
        return false;
    if(strstr(json_object_get_string(found), current->filter_data))
        return true;
    return false;
}

static bool
_filter_noop(UNUSED bool jpointer, UNUSED json_object *found,
    UNUSED Needles current)
{
    return true;
}

static bool
_filter_exists(bool jpointer, UNUSED json_object *found,
    UNUSED Needles current)
{
    return jpointer;
}

static bool
_json_to_pqtext(json_object *found, Needles current)
{
    // Any json type can be cast to string
    current->result = (char *)json_object_get_string(found);
    current->length = strlen(current->result);
    return true;
}

static bool
_json_to_pqtimestamp(json_object *found, Needles current)
{
    const char *ts = json_object_get_string(found);
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

    //2019-11-05T11:31:34Z
    #define T_MONTH 5
    #define T_DAY 8
    #define T_HOUR 11
    #define T_MINUTE 14
    #define T_SECOND 17
    #define T_FRACTION 20
    #define T_MIN 20
    #define T_MAX 31

    // postgres only stores 6 digits after .
    #define PG_FRACTION 6

    if (len < T_MIN || len > T_MAX) {
        logger_log("%s %d: Datestring %s not supported",
            __FILE__, __LINE__, ts);
        goto error;
    }

    // If a timestamp is not like 2000-01-01T00:00:01.000000Z
    // or 2000-01-01T:00:00:01Z it's considered invalid
    // Z stands for Zulu/UTC
    if (ts[T_MONTH-1] != '-' || ts[T_DAY-1] != '-' || ts[T_HOUR-1] != 'T'
            || ts[T_MINUTE-1] != ':' || ts[T_SECOND-1] != ':'
            || !(ts[T_FRACTION-1] == '.' || ts[T_FRACTION-1] == 'Z')
            || ts[len-1] != 'Z') {
        logger_log("%s %d: Datestring %s not supported",
            __FILE__, __LINE__, ts);
        goto error;
    }

    errno = 0;
    tm.year = strtoul(ts,NULL,10);
    tm.month = strtoul(ts+T_MONTH,NULL,10);
    tm.day = strtoul(ts+T_DAY,NULL,10);
    tm.hour = strtoul(ts+T_HOUR,NULL,10);
    tm.minute = strtoul(ts+T_MINUTE,NULL,10);
    tm.second = strtoul(ts+T_SECOND,NULL,10);
    if(ts[T_FRACTION] != 'Z' && ts[T_FRACTION-1] != 'Z') { // fractionless timestamps
        char micro[PG_FRACTION+1] = "000000";
        size_t bytes = (len-1) - T_MIN > PG_FRACTION ?
            PG_FRACTION : (len-1) - T_MIN;
        memcpy(micro, ts+T_MIN,bytes);
        tm.micro = strtoull(micro,NULL,10);
    }

    if(errno) {
        logger_log("%s %d: Error %s in date conversion",
            __FILE__, __LINE__, strerror(errno));
        goto error;
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
        } else if (i < 8) {
            tm.yday += 30 + (i%2);
        } else {
            // 31sts occure on even months from august on. I curse you pope gregory XIII.
            tm.yday += 30 + ((i+1)%2);
        }

    }

    tm.yday += tm.day;

    epoch = tm.second + tm.minute*60 + tm.hour*3600 + (tm.yday-1)*86400
    + *(current->leapyears+(tm.year))*86400 + tm.year*31536000;

    epoch *= 1000000;
    epoch += tm.micro;

    *((uint64_t *) current->result) = htobe64(epoch);
    current->length = sizeof(uint64_t);

    return true;
    error:
    return false;
}

Needles *
transform_needles(config_setting_t *needlestack, Internal internal)
{
    config_setting_t *setting = NULL, *member= NULL;
    int list = 0;
    list = config_setting_length(needlestack);

    Needles *needles = SCALLOC(list,sizeof(*needles));
    internal->leapyears = _leapyear();

    internal->rows = 0;

    for(int i = 0; i < list; i++) {
        const ExportsAction *actiontype;
        const ExportsFilter *filtertype;
        const PqType        *pqtype;
        Needles              current = SCALLOC(list,sizeof(*current));

        needles[i] = current;
        // Alias should a needle need leapyear data
        needles[i]->leapyears = internal->leapyears;

        setting = config_setting_get_elem(needlestack, i);
        if (config_setting_type(setting) != CONFIG_TYPE_ARRAY) {
            logger_log("%s %d: data structure not an array", __FILE__, __LINE__);
            abort();
        }

        member = config_setting_get_elem(setting, 0);
        current->jpointer = strdup(config_setting_get_string(member));
        if (!current->jpointer) {
            logger_log("%s %d: Failed to strdup", __FILE__, __LINE__);
            abort();
        }

        member = config_setting_get_elem(setting, 1);
        pqtype = lookup_pqtype(config_setting_get_string(member));
        current->format = pqtype->format;
        current->free = pqtype->free;

        member = config_setting_get_elem(setting, 2);
        actiontype = lookup_actiontype(config_setting_get_string(member));
        current->action = actiontype->action;
        current->store = actiontype->store;
        if (current->store)
            internal->rows++;

        member = config_setting_get_elem(setting, 3);
        filtertype = lookup_filtertype(config_setting_get_string(member));
        current->filter = filtertype->filter;
        if(filtertype->needs_data)
            current->filter_data =
                config_setting_get_string(config_setting_get_elem(setting, 4));

    }

    return needles;
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
    m->cpycmd = _cpycmd(host, topic);
    m->conninfo = _connectinfo(host);
    m->internal = i;
    m->internal->needles = transform_needles(needlestack, i);
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
    Internal internal = (*m)->internal;
    pthread_mutex_destroy(&(*m)->commit_mutex);

    free((*m)->conninfo);
    free((*m)->cpycmd);
    PQfinish((*m)->conn_master);

    for (int i = 0; i < internal->ncount; i++ ) {
        free(internal->needles[i]->jpointer);
        internal->needles[i]->free(
            &(internal->needles[i]->result));
        free(internal->needles[i]);
    }
    free(internal->needles);
    free(internal->leapyears);
    free(internal);

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

int
extract_needles_from_haystack(json_object *haystack, Internal internal)
{
    json_object *found;

    Needles *needles = internal->needles;
    for (int i = 0; i < internal->ncount; i++) {
        found = NULL;
        /* json_pointer_get returns negative if an error (or not found)
         * returns 0 if it succeeds */
        int ret = json_pointer_get(haystack, needles[i]->jpointer, &found);

        if(!needles[i]->action(
                needles[i]->filter(ret == 0, found, needles[i]),
                found, needles[i]))
            return 1;

        if(ret) {
            // if json_pointer returned null
            needles[i]->result = NULL;
            // complement of 0 is uint32_t max
            // int max is postgres NULL;
            needles[i]->length = (uint32_t)~0;
        } else {
            if(!needles[i]->format(found, needles[i])) {
                logger_log("%s %d: Failed jpointer deref %s",
                    __FILE__, __LINE__, needles[i]->jpointer);
                return -1;
            }
        }
    }

    return 0;
}

static void
exports_producer_produce(Producer p, Message msg)
{
    Meta m = (Meta)p->meta;
    Needles *needles = m->internal->needles;
    Internal internal = m->internal;
    json_object *haystack = NULL;
    int ret = 0;

    size_t len = message_get_len(msg);
    char* data = message_get_data(msg);

    // postgres is big endian internally
    uint16_t rows = htons(m->internal->rows);

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

    // get value from json, apply transformation
    if((ret = extract_needles_from_haystack(haystack, internal) != 0)) {
        if(ret == -1)
            logger_log("%s %d: Failed to dereference json!\n %s",
                __FILE__, __LINE__, data);
        for (int i = 0; i < internal->ncount; i++) {
            needles[i]->free(&needles[i]->result);
        }
        goto fail;
    }

    PQputCopyData(m->conn_master, (char *) &rows, 2);

    for (int i = 0; i < internal->ncount; i++) {
        if(!needles[i]->store)
            continue;
        uint32_t length =  htobe32(needles[i]->length);
        PQputCopyData(m->conn_master, (void *) &length, 4);

        if(needles[i]->result) {
            PQputCopyData(m->conn_master, needles[i]->result, needles[i]->length);
            needles[i]->free(&needles[i]->result);
        }
    }

    m->count = m->count + 1;
    if (m->count == 2000)
    {
        commit(&m);
    }

    error:
    fail:
    json_object_put(haystack);
    pthread_mutex_unlock(&m->commit_mutex);
}

static void
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

static int
exports_consumer_consume(UNUSED Consumer c, UNUSED Message msg)
{
    //TODO
    return -1;
}

static void
exports_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    exports_meta_free(&m);
    free(*c);
    *c = NULL;
}

bool
validate_jpointers(config_setting_t *setting)
{
    if(!CONF_IS_LIST(setting, "require jpointers to be a list!"))
        return false;

    /* We need to create an array which holds jpointer, pqtype, action
     * and filter. We do this by taking the first element, deleting it
     * and appending a completely populated element at the end.
     */
    for (int i = 0; i < config_setting_length(setting); i++)
    {
        const char *jpointer = NULL,
                   *pqtype = "text",
                   *action = "store",
                   *filter = "noop",
                   *data = "",
                   *conf = NULL;

        config_setting_t   *member = NULL,
                           *child = NULL,
                           *new = NULL;

        child = config_setting_get_elem(setting, 0);
        if (child == NULL) {
            fprintf(stderr, "%s %d: failed to read jpointer list elem %d\n",
                    __FILE__, __LINE__, i);
            return false;;
        }

        new = config_setting_add(setting, NULL, CONFIG_TYPE_ARRAY);
        for (int j = 0; j < 5; j++ )
            config_setting_add(new, NULL, CONFIG_TYPE_STRING);

        if (config_setting_is_scalar(child) == CONFIG_TRUE) {
            conf = config_setting_get_string(child);
            if(conf == NULL) {
                fprintf(stderr, "%s %d: couldn't parse config at %u\n",
                __FILE__, __LINE__, config_setting_source_line(child));
                return false;
            }
            jpointer = conf;
        } else if (config_setting_is_array(child) == CONFIG_TRUE) {
            member = config_setting_get_elem(child, 0);
            if(member == NULL) {
                fprintf(stderr, "%s %d: failed to read jpointer array %d\n",
                        __FILE__, __LINE__, i);
                return false;
            }
            conf = config_setting_get_string(member);
            if(conf == NULL) {
                fprintf(stderr, "%s %d: couldn't parse config at %u\n",
                        __FILE__, __LINE__, config_setting_source_line(child));
                return false;
            }
            jpointer = conf;

            member = config_setting_get_elem(child, 1);
            if(member != NULL) {
                conf = config_setting_get_string(member);
                if(conf != NULL && !lookup_pqtype(conf)) {
                    fprintf(stderr, "%s %d: not a valid type transformation: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                } else
                    pqtype = conf;
            }

            member = config_setting_get_elem(child, 2);
            if(member != NULL) {
                conf = config_setting_get_string(member);
                if( conf != NULL && !lookup_actiontype(conf)) {
                    fprintf(stderr, "%s %d: not a valid action type: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                } else
                    action = conf;
            }

            member = config_setting_get_elem(child, 3);
            if(member != NULL) {
                conf = config_setting_get_string(member);
                if( conf != NULL && !lookup_filtertype(conf)) {
                    fprintf(stderr, "%s %d: not a valid filter type: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                } else
                    filter = conf;
            }

            if(lookup_filtertype(filter)->needs_data) {
                member = config_setting_get_elem(child,4);
                if(member == NULL) {
                    fprintf(stderr, "%s %d: filter needs configuration: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                }
                conf = config_setting_get_string(member);
                if(conf == NULL) {
                    fprintf(stderr, "%s %d: filter needs configuration: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                }
                    data = conf;
            }
        } else if (config_setting_is_group(child) == CONFIG_TRUE) {
            if(!CONF_L_IS_STRING(child, "jpointer", &jpointer, "failed to parse config"))
                return false;

            if(config_setting_lookup_string(child, "pqtype", &conf) == CONFIG_TRUE) {
                if (!lookup_pqtype(conf)) {
                    fprintf(stderr, "%s %d: not a valid type transformation: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                }
                pqtype = conf;
            }

            if(config_setting_lookup_string(child, "action", &conf) == CONFIG_TRUE) {
                if (!lookup_actiontype(conf)) {
                    fprintf(stderr, "%s %d: not a valid action type: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                }
                action = conf;
            }

            if(config_setting_lookup_string(child, "filter", &conf) == CONFIG_TRUE) {
                if (!lookup_filtertype(conf)) {
                    fprintf(stderr, "%s %d: not a valid filter type: %s\n",
                            __FILE__, __LINE__, conf);
                    return false;
                }
                filter = conf;
            }

            if(lookup_filtertype(filter)->needs_data) {
                if(config_setting_lookup_string(child, "data", &conf) == CONFIG_TRUE)
                    data = conf;
                else {
                    fprintf(stderr, "%s %d: filter needs configuration: %s",
                            __FILE__, __LINE__, conf);
                    return false;
                }
            }
        } else {
            fprintf(stderr, "%s %d: jpointer needs to be a "
            "string/array/group\n", __FILE__, __LINE__);
        }
        config_setting_set_string_elem(new, 0, jpointer);
        config_setting_set_string_elem(new, 1, pqtype);
        config_setting_set_string_elem(new, 2, action);
        config_setting_set_string_elem(new, 3, filter);
        config_setting_set_string_elem(new, 4, data);
        config_setting_remove_elem(setting,0);
    }

    return true;
}

bool
exporter_validate(UNUSED config_setting_t *config)
{
    config_setting_t *setting = NULL;
    const char *host = NULL, *topic = NULL;

    if(!CONF_L_IS_STRING(config, "host", &host, "require host string!"))
        return false;

    if(!CONF_L_IS_STRING(config, "topic", &topic, "require topic string!"))
        return false;

    if(!(setting = CONF_GET_MEM(config,"jpointers", "require jpointers!")))
        return false;

    if(!validate_jpointers(setting))
        return false;

    return true;
}


Validator
exports_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = &exporter_validate;
    v->validate_producer = &exporter_validate;
    return v;
}
