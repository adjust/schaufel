#include <errno.h>
#include <libpq-fe.h>
// #include <pthread.h>
#include <threads.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <arpa/inet.h>

#include "bagger.h"
#include "utils/config.h"
#include "utils/helper.h"
#include "utils/logger.h"
#include "utils/postgres.h"
#include "utils/scalloc.h"
#include "utils/endian.h"
#include "utils/htable.h"


#define min(a, b) ((a) < (b) ? (a) : (b))

typedef struct Needles *Needles;
typedef struct Needles {
    char           *jpointer;
    bool          (*format) (json_object *, Needles);
    void          (*free) (void **);
    uint32_t       *leapyears;
    uint32_t        length;
    void           *result; // output
    bool          (*action) (bool, json_object *, Needles);
    bool          (*filter) (bool, json_object *, Needles);
    bool            store;
    const char     *filter_data;
} *Needles;

typedef struct Internal {
    Needles        *needles;
    uint32_t       *leapyears;  // leapyears are globally shared
    uint16_t        ncount; // count of needles
    uint16_t        rows; // number of rows inserted into postgres
    HTable         *htable; // hashtable of per-table buffers
} *Internal;

typedef struct BaggerMeta
{
    PGconn         *conn;
    mtx_t           commit_mtx;
    thrd_t          commit_thrd;
    _Atomic bool    commit_thrd_exit;
    Internal        internal;
    int             count;
} BaggerMeta;

#define BUFFER_SIZE 1024

typedef struct BufferNode BufferNode;

typedef struct Buffer {
    BufferNode     *buf_head;   // buffers list
    BufferNode     *buf_tail;   // last buffers list element
    size_t          nbytes;     // bytes actually written
} Buffer;

typedef struct BufferNode {
    char            data[BUFFER_SIZE];
    BufferNode     *next;
} BufferNode;

static void bagger_producer_free(Producer *p);
static void bagger_producer_produce(Producer p, Message msg);
static void bagger_consumer_free(Consumer *c);
static int bagger_consumer_consume(Consumer c, Message msg);

static void buffer_write(Buffer *buf, const char *data, size_t len);

static bool _json_to_pqtext (json_object *needle, Needles current);
static bool _json_to_pqtimestamp (json_object *needle, Needles current);
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

static char *_cpycmd(const char *table);

static int bagger_commit_worker(void *);

// Types of postgres fields
typedef enum {  // todo: add jsonb, int.
    pqtype_undef,
    pqtype_text,
    pqtype_timestamp,
} PqTypes;

static const struct {
    PqTypes type;
    const char *pq_type;
    bool  (*format) (json_object *, Needles);
    void  (*free) (void **obj);
}  pq_types [] = {
        {pqtype_undef, "undef", NULL, NULL},
        {pqtype_text, "text", &_json_to_pqtext, &_obj_noop},
        {pqtype_timestamp, "timestamp", &_json_to_pqtimestamp, &_obj_free},
};


// Types of actions available for data
typedef enum {
    action_undef,           // invalid conf
    action_store,           // store the field/null whatever happens
    action_store_true,      // store field if filter returns true
    action_discard_false,   // discard the message if filter returns false
    action_discard_true,    // discard the message if filter returns true
} ActionTypes;

static const struct {
    ActionTypes type;
    const char *action_type;
    bool  (*action) (bool, json_object *, Needles);
    bool store;
}  action_types [] = {
        {action_undef, "undef", NULL, false},
        {action_store, "store", &_action_store, true},
        {action_store_true, "store_true", &_action_store_true, true},
        {action_discard_false, "discard_false", &_action_discard_false, false},
        {action_discard_true, "discard_true", &_action_discard_true, false}
};

// Types of filters to be applied on data
typedef enum {
    filter_undef,
    filter_noop,        // returns true, default
    filter_match,       // match string
    filter_substr,      // match substring
    filter_exists,      // json key exists
// TODO:    filter_pcrematch,   // pcre match
} FilterTypes;

static const struct {
    FilterTypes type;
    const char *filter_type;
    bool (*filter) (bool, json_object *, Needles);
    bool needs_data;
}  filter_types [] = {
        {filter_undef, "undef", NULL, false},
        {filter_noop, "noop", &_filter_noop, false},
        {filter_match, "match", &_filter_match, true},
        {filter_substr, "substr", &_filter_substr, true},
        {filter_exists, "exists", &_filter_exists, false},
// TODO:        {filter_pcrematch, "pcrematch", NULL, true},
};

static void
buffer_write(Buffer *buf, const char *data, size_t len)
{
    size_t      offset;
    size_t      bytes_left;
    BufferNode *node = buf->buf_tail;

    if (!node)
    {
        // initialize buffer
        node = SCALLOC(1, sizeof(BufferNode));
        buf->buf_head = node;
        buf->buf_tail = node;
    }

    // calculate free space in the last buffer node;
    offset = buf->nbytes % BUFFER_SIZE;
    bytes_left = BUFFER_SIZE - offset;

    while (true)  {
        size_t  bytes_copied = min(bytes_left, len);

        memcpy(node->data + offset, data, bytes_copied);

        data += bytes_copied;
        len -= bytes_copied;
        buf->nbytes += bytes_copied;

        // Do we need more space? Allocate a new buffer node
        if (len > 0) {
            BufferNode *new_node = SCALLOC(1, sizeof(BufferNode));

            node->next = new_node;
            node = new_node;
            buf->buf_tail = new_node;

            bytes_left = BUFFER_SIZE;
            offset = 0;
        } else {
            break;
        }
    };
}

static void
buffer_flush(BaggerMeta *m, Buffer *buf, const char *tablename)
{
    char       *cpycmd;
    size_t      nbytes;
    BufferNode *node,
               *prev;
    PGresult   *res;

    cpycmd = _cpycmd(tablename);

    res = PQexec(m->conn, cpycmd);
    if (PQresultStatus(res) != PGRES_COPY_IN)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn));
        abort();
    }
    PQclear(res);
    free(cpycmd);

    // Send signature, flags and header extension length
    // TODO: check return value
    PQputCopyData(m->conn,
        "PGCOPY\n\377\r\n\0" //postgres magic
        "\0\0\0\0"  // flags field (only bit 16 relevant)
        "\0\0\0\0"  // Header extension area
        , 19);

    nbytes = buf->nbytes;

    // write out buffers
    node = buf->buf_head;
    while (node != NULL)
    {
        size_t bufsize = min(nbytes, BUFFER_SIZE);

        nbytes -= bufsize;
        PQputCopyData(m->conn, node->data, bufsize);

        prev = node;
        node = node->next;

        free(prev);
    }

    // finish write
    // TODO: check return values
    PQputCopyData(m->conn, "\377\377", 2);
    PQputCopyEnd(m->conn, NULL);

    // mtx_unlock(&m->commit_mtx);
}

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

static ActionTypes
_actiontype_enum(const char *action_type)
{
    uint32_t j;
    for (j = 1; j < sizeof(action_types) / sizeof(action_types[0]); j++)
        if (!strcmp(action_type, action_types[j].action_type))
            return action_types[j].type;
    return action_undef;
}


static FilterTypes
_filtertype_enum(const char *filter_type)
{
    uint32_t j;
    for (j = 1; j < sizeof(filter_types) / sizeof(filter_types[0]); j++)
        if (!strcmp(filter_type, filter_types[j].filter_type))
            return filter_types[j].type;
    return filter_undef;
}

static PqTypes
_pqtype_enum(const char *pqtype)
{
    uint32_t j;
    for (j = 1; j < sizeof(pq_types) / sizeof(pq_types[0]); j++)
        if (!strcmp(pqtype, pq_types[j].pq_type))
            return pq_types[j].type;
    return pqtype_undef;
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

static Needles *
_needles(config_setting_t *needlestack, Internal internal)
{
    config_setting_t *setting = NULL, *member= NULL;
    int list = 0, pqtype = 0, actiontype = 0, filtertype = 0;
    list = config_setting_length(needlestack);

    Needles *needles = SCALLOC(list,sizeof(*needles));
    internal->leapyears = _leapyear();

    internal->rows = 0;

    for(int i = 0; i < list; i++) {
        Needles current = SCALLOC(list,sizeof(*current));
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
        pqtype = _pqtype_enum(config_setting_get_string(member));
        current->format = pq_types[pqtype].format;
        current->free = pq_types[pqtype].free;

        member = config_setting_get_elem(setting, 2);
        actiontype = _actiontype_enum(config_setting_get_string(member));
        current->action = action_types[actiontype].action;
        current->store = action_types[actiontype].store;
        if (current->store)
            internal->rows++;

        member = config_setting_get_elem(setting, 3);
        filtertype = _filtertype_enum(config_setting_get_string(member));
        current->filter = filter_types[filtertype].filter;
        if(filter_types[filtertype].needs_data)
            current->filter_data =
            config_setting_get_string(config_setting_get_elem(setting, 4));

    }

    return needles;
}

static char *
_cpycmd(const char *table)
{
    const char *fmtstring = "COPY %s FROM STDIN (FORMAT binary)";

    int len = strlen(table)
            + strlen(fmtstring);

    char *cpycmd = SCALLOC(len + 1, sizeof(*cpycmd));

    snprintf(cpycmd, len, fmtstring, table);
    return cpycmd;
}

static uint32_t
_hashfunc(const char *key)
{
    const char *s = key;
    uint32_t    hash = 0;

    while(*s)
    {
        hash += *s;
        s++;
    }

    return hash;
}

BaggerMeta *
bagger_meta_init(const char *host, const char *topic, config_setting_t *needlestack)
{
    BaggerMeta     *m = SCALLOC(1, sizeof(*m));
    Internal        i = SCALLOC(1, sizeof(*i));
    char           *conninfo;

    conninfo = _connectinfo(host);

    m->internal = i;
    m->internal->needles = _needles(needlestack, i);
    m->internal->ncount = config_setting_length(needlestack);
    m->internal->htable = ht_create(16, sizeof(Buffer), _hashfunc);

    m->conn = PQconnectdb(conninfo);
    if (PQstatus(m->conn) != CONNECTION_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->conn));
        abort();
    }
    free(conninfo);

    if (mtx_init(&m->commit_mtx, mtx_plain) != thrd_success) {
        logger_log("%s %d: unable to create mutex", __FILE__, __LINE__ );
        abort();
    }

    return m;
}

void
bagger_meta_free(BaggerMeta *m)
{
    Internal internal = m->internal;
    mtx_destroy(&m->commit_mtx);

    // free(m->conninfo);
    // free(m->cpycmd);
    PQfinish(m->conn);

    for (int i = 0; i < internal->ncount; i++ ) {
        free(internal->needles[i]->jpointer);
        internal->needles[i]->free(
            &(internal->needles[i]->result));
        free(internal->needles[i]);
    }
    free(internal->needles);
    free(internal->leapyears);
    free(internal->htable);
    free(internal);

    free(m);
}

Producer
bagger_producer_init(config_setting_t *config)
{
    const char *host = NULL, *topic = NULL;
    config_setting_t *needlestack = NULL;
    config_setting_lookup_string(config, "host", &host);
    config_setting_lookup_string(config, "topic", &topic);
    needlestack = config_setting_get_member(config, "jpointers");
    Producer bagger = SCALLOC(1, sizeof(*bagger));

    bagger->meta          = bagger_meta_init(host, topic, needlestack);
    bagger->producer_free = bagger_producer_free;
    bagger->produce       = bagger_producer_produce;

    if (thrd_create(&((BaggerMeta *) bagger->meta)->commit_thrd,
                    bagger_commit_worker,
                    bagger->meta) != thrd_success) {
        logger_log("%s %d: Failed to create commit worker!", __FILE__, __LINE__);
        abort();
    }

#if 0
    if (pthread_create(&((Meta)(exports->meta))->commit_worker,
        NULL,
        commit_worker,
        (void *)&(exports->meta))) {
        logger_log("%s %d: Failed to create commit worker!", __FILE__, __LINE__);
        abort();
    }
#endif

    return bagger;
}

static int
_deref(json_object *haystack, Internal internal)
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

// partition_name
//      Generates table name based on values in the `in` object. The input json
//      must contain these three attributes: "service", "tag", "timestamp".
//      The result is written into the `out` string. String must be allocated
//      by the caller and be at least 64 bytes long (postgres naming rules
//      require names to be no longer than 64 bytes including terminal zero)
static void
partition_name(json_object *in, char *out)
{
    json_object    *obj;
    int             i;
    const char     *names[] = {"/service", "/tag", "/timestamp"};
    const char     *values[3];
    char           *s;

#if 0
    strcpy(out, "stub");
#endif
    for (i = 0; i < 3; ++i)
    {
        if (json_pointer_get(in, names[i], &obj) < 0)
        {
            logger_log("%s %d: invalid json, \"%s\" not found",
                       __FILE__, __LINE__, names[i]);
            abort();
        }
        values[i] = json_object_get_string(obj);
    }

    snprintf(out, 64, "data_%s_%s_%s", values[0], values[1], values[2]);

    // Replace invalid charachters (according to postgres naming convention).
    // We assume for now that the only invalid charachers could be in timestamp
    // string. Feel free to extend this part if situation changes.
    s = out;
    while (*s)
    {
        if (*s == '-')
            *s = '_';
        s++;
    }
}

void
bagger_producer_produce(Producer p, Message msg)
{
    BaggerMeta *m = (BaggerMeta *) p->meta;
    Needles *needles = m->internal->needles;
    Internal internal = m->internal;
    json_object *haystack = NULL;
    char        tablename[64]; // max table name in postgres including \0
    Buffer     *buf;
    int         ret = 0;

    size_t len = message_get_len(msg);
    char* data = message_get_data(msg);

    // postgres is big endian internally
    uint16_t rows = htons(m->internal->rows);

    if (data[len] != '\0')
    {
        logger_log("payload doesn't end on null terminator");
        return;
    }

    mtx_lock(&m->commit_mtx);
#if 0
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
#endif

    haystack = json_tokener_parse(data);
    if(!haystack) {
        logger_log("%s %d: Failed to tokenize json!", __FILE__, __LINE__);
        goto error;
    }

    partition_name(haystack, tablename);
    buf = ht_search(internal->htable, tablename, HT_UPSERT);

    // get value from json, apply transformation
    if((ret = _deref(haystack, internal) != 0)) {
        if(ret == -1)
            logger_log("%s %d: Failed to dereference json!\n %s",
                __FILE__, __LINE__, data);
        for (int i = 0; i < internal->ncount; i++) {
            needles[i]->free(&needles[i]->result);
        }
        goto fail;
    }

    // PQputCopyData(m->conn_master, (char *) &rows, 2);
    buffer_write(buf, (char *) &rows, 2);

    for (int i = 0; i < internal->ncount; i++) {
        if(!needles[i]->store)
            continue;
        uint32_t length =  htobe32(needles[i]->length);
        // PQputCopyData(m->conn, (void *) &length, 4);
        buffer_write(buf, (char *) &length, 4);

        if(needles[i]->result) {
            // PQputCopyData(m->conn, needles[i]->result, needles[i]->length);
            buffer_write(buf, needles[i]->result, needles[i]->length);
            needles[i]->free(&needles[i]->result);
        }
    }

    m->count = m->count + 1;
    if (m->count == 2000)
    {
        // commit(&m);
        buffer_flush(m, buf, tablename);
    }

    error:
    fail:
    json_object_put(haystack);

    mtx_unlock(&m->commit_mtx);
}

void
bagger_producer_free(Producer *p)
{
    BaggerMeta *m = (BaggerMeta *) ((*p)->meta);
    HTable     *ht = m->internal->htable;
    HTIter     *iter;
    Buffer     *buf;
    const char *key;
    int         res;

    m->commit_thrd_exit = true;
    thrd_join(m->commit_thrd, &res);

    // flush all remaining messages
    iter = ht_iter_create(ht);
    while ((key = ht_iter_next(iter, (void **) &buf)) != NULL)
    {
        buffer_flush(m, buf, key);

        /*
         * With the way iterator is implemented it's safe to remove
         * items as we iterate.
         */
        ht_search(ht, key, HT_REMOVE);
    }
    free(iter);

    bagger_meta_free(m);
    free(*p);
    *p = NULL;
}

Consumer
bagger_consumer_init(config_setting_t *config)
{
    const char *host = NULL;
    config_setting_t *needles = NULL;
    config_setting_lookup_string(config, "host", &host);
    Consumer bagger = SCALLOC(1, sizeof(*bagger));

    bagger->meta          = bagger_meta_init(host, NULL, needles);
    bagger->consumer_free = bagger_consumer_free;
    bagger->consume       = bagger_consumer_consume;

    return bagger;
}

int
bagger_consumer_consume(UNUSED Consumer c, UNUSED Message msg)
{
    //TODO
    return -1;
}

void
bagger_consumer_free(Consumer *c)
{
    BaggerMeta *m = (BaggerMeta *) ((*c)->meta);
    bagger_meta_free(m);
    free(*c);
    *c = NULL;
}

bool
bagger_validate(UNUSED config_setting_t *config)
{
    config_setting_t *setting = NULL, *member = NULL, *child = NULL,
        *new = NULL;
    const char *jpointer = NULL, *pqtype = NULL, *action = NULL, *filter = NULL,
        *data = NULL, *conf = NULL;

    const char *host = NULL, *topic = NULL;
    if(!CONF_L_IS_STRING(config, "host", &host, "require host string!"))
        goto error;

    if(!CONF_L_IS_STRING(config, "topic", &topic, "require topic string!"))
        goto error;

    if(!(setting = CONF_GET_MEM(config,"jpointers", "require jpointers!")))
        goto error;

    if(!CONF_IS_LIST(setting, "require jpointers to be a list!"))
        goto error;

    /* We need to create an array which holds jpointer, pqtype, action
     * and filter. We do this by taking the first element, deleting it
     * and appending a completely populated element at the end.
     */
    for (int i = 0; i < config_setting_length(setting); i++) {
        child = config_setting_get_elem(setting, 0);
        if (child == NULL) {
            fprintf(stderr, "%s %d: failed to read jpointer list elem %d\n",
            __FILE__, __LINE__, i);
            goto error;
        }

        new = config_setting_add(setting, NULL, CONFIG_TYPE_ARRAY);
        for (int j = 0; j < 5; j++ )
            config_setting_add(new, NULL, CONFIG_TYPE_STRING);

        jpointer = NULL;
        pqtype = "text";
        action = "store";
        filter = "noop";
        data = "";

        if (config_setting_is_scalar(child) == CONFIG_TRUE) {
            conf = config_setting_get_string(child);
            if(conf == NULL) {
                fprintf(stderr, "%s %d: couldn't parse config at %u\n",
                __FILE__, __LINE__, config_setting_source_line(child));
                goto error;
            }
            jpointer = conf;
        } else if (config_setting_is_array(child) == CONFIG_TRUE) {
            member = config_setting_get_elem(child, 0);
            if(member == NULL) {
                fprintf(stderr, "%s %d: failed to read jpointer array %d\n",
                __FILE__, __LINE__, i);
                goto error;
            }
            conf = config_setting_get_string(member);
            if(conf == NULL) {
                fprintf(stderr, "%s %d: couldn't parse config at %u\n",
                __FILE__, __LINE__, config_setting_source_line(child));
                goto error;
            }
            jpointer = conf;

            member = config_setting_get_elem(child, 1);
            if(member != NULL) {
                conf = config_setting_get_string(member);
                if(conf != NULL && !_pqtype_enum(conf)) {
                    fprintf(stderr, "%s %d: not a valid type transformation: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                } else
                    pqtype = conf;
            }

            member = config_setting_get_elem(child, 2);
            if(member != NULL) {
                conf = config_setting_get_string(member);
                if( conf != NULL && !_actiontype_enum(conf)) {
                    fprintf(stderr, "%s %d: not a valid action type: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                } else
                    action = conf;
            }

            member = config_setting_get_elem(child, 3);
            if(member != NULL) {
                conf = config_setting_get_string(member);
                if( conf != NULL && !_filtertype_enum(conf)) {
                    fprintf(stderr, "%s %d: not a valid filter type: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                } else
                    filter = conf;
            }

            if(filter_types[_filtertype_enum(filter)].needs_data) {
                member = config_setting_get_elem(child,4);
                if(member == NULL) {
                    fprintf(stderr, "%s %d: filter needs configuration: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                }
                conf = config_setting_get_string(member);
                if(conf == NULL) {
                    fprintf(stderr, "%s %d: filter needs configuration: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                }
                    data = conf;
            }
        } else if (config_setting_is_group(child) == CONFIG_TRUE) {
            if(!CONF_L_IS_STRING(child, "jpointer", &jpointer, "failed to parse config"))
                goto error;

            if(config_setting_lookup_string(child, "pqtype", &conf) == CONFIG_TRUE) {
                if (!_pqtype_enum(conf)) {
                    fprintf(stderr, "%s %d: not a valid type transformation: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                }
                pqtype = conf;
            }

            if(config_setting_lookup_string(child, "action", &conf) == CONFIG_TRUE) {
                if (!_actiontype_enum(conf)) {
                    fprintf(stderr, "%s %d: not a valid action type: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                }
                action = conf;
            }

            if(config_setting_lookup_string(child, "filter", &conf) == CONFIG_TRUE) {
                if (!_filtertype_enum(conf)) {
                    fprintf(stderr, "%s %d: not a valid filter type: %s\n",
                    __FILE__, __LINE__, conf);
                    goto error;
                }
                filter = conf;
            }

            if(filter_types[_filtertype_enum(filter)].needs_data) {
                if(config_setting_lookup_string(child, "data", &conf) == CONFIG_TRUE)
                    data = conf;
                else {
                    fprintf(stderr, "%s %d: filter needs configuration: %s",
                    __FILE__, __LINE__, conf);
                    goto error;
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
    error:
    return false;
}


Validator
bagger_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = &bagger_validate;
    v->validate_producer = &bagger_validate;
    return v;
}

int
bagger_commit_worker(void *meta)
{
    BaggerMeta *m = (BaggerMeta *) meta;

#ifdef PR_SET_NAME
    prctl(PR_SET_NAME, "commit_worker");
#endif

    while(42)
    {
        HTable     *ht = m->internal->htable;
        HTIter     *iter;
        Buffer     *buf;
        const char *key;

        sleep(5);

        if (m->commit_thrd_exit)
            break;

        mtx_lock(&m->commit_mtx);

        iter = ht_iter_create(ht);
        while ((key = ht_iter_next(iter, (void **) &buf)) != NULL)
        {
            buffer_flush(m, buf, key);

            /*
             * With the way iterator is implemented it's safe to remove
             * items as we iterate.
             */
            ht_search(ht, key, HT_REMOVE);
        }
        free(iter);

#if 0
        m->commit_iter++;
        m->commit_iter &= 0xF;

        /* if count > 0 it implies that copy == 1,
         * therefore it is safe to commit data */
        if((!((*m)->commit_iter)) && ((*m)->count > 0)) {
            logger_log("%s %d: Autocommiting %d entries",
                __FILE__, __LINE__, (*m)->count);
            commit(m);
        }

        // pthread_cleanup_pop(0);
#endif

        mtx_unlock(&m->commit_mtx);
        // pthread_testcancel();
    }

    return 0;
}

