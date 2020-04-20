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
#include "utils/murmur.h"
#include "exports.h"


#define min(a, b) ((a) < (b) ? (a) : (b))

typedef struct BaggerMeta
{
    PGconn         *conn;
    mtx_t           commit_mtx;
    thrd_t          commit_thrd;
    _Atomic bool    commit_thrd_exit;
    Internal        internal;
    int             count;
    HTable         *htable; // hashtable of per-table buffers
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

static char *_cpycmd(const char *table);

static int bagger_commit_worker(void *);

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

    // calculate remaining space in the buffer node
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

    nbytes = buf->nbytes;

    // write out buffers
    node = buf->buf_head;
    while (node != NULL)
    {
        size_t bufsize = min(nbytes, BUFFER_SIZE);

        nbytes -= bufsize;
        if (PQputCopyData(m->conn, node->data, bufsize) < 0)
        {
            logger_log("%s %d: PQputCopyData failed!\n %s",
                       __FILE__, __LINE__);
            abort();
        }

        prev = node;
        node = node->next;

        free(prev);
    }

    // finish write
    // TODO: check return values
    PQputCopyData(m->conn, "\\.\n", 3);
    PQputCopyEnd(m->conn, NULL);
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

static char *
_cpycmd(const char *table)
{
    const char *fmtstring = "COPY %s FROM STDIN (FORMAT text)";

    int len = strlen(table)
            + strlen(fmtstring);

    char *cpycmd = SCALLOC(len + 1, sizeof(*cpycmd));

    snprintf(cpycmd, len, fmtstring, table);
    return cpycmd;
}

static uint32_t
hashfunc(const char *key)
{
    return MurmurHash2(key, strlen(key), 0);
}

BaggerMeta *
bagger_meta_init(const char *host, const char *topic, config_setting_t *needlestack)
{
    BaggerMeta     *m = SCALLOC(1, sizeof(*m));
    Internal        i = SCALLOC(1, sizeof(*i));
    char           *conninfo;

    conninfo = _connectinfo(host);

    m->internal = i;
    m->internal->needles = transform_needles(needlestack, i);
    m->internal->ncount = config_setting_length(needlestack);
    m->htable = ht_create(16, sizeof(Buffer), hashfunc);

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

    PQfinish(m->conn);

    for (int i = 0; i < internal->ncount; i++ ) {
        free(internal->needles[i]->jpointer);
        internal->needles[i]->free(
            &(internal->needles[i]->result));
        free(internal->needles[i]);
    }
    free(internal->needles);
    free(internal->leapyears);
    free(m->htable);
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

    return bagger;
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
    uint16_t rows = htons(m->internal->rows + 1);

    if (data[len] != '\0')
    {
        logger_log("payload doesn't end on null terminator");
        return;
    }

    mtx_lock(&m->commit_mtx);

    haystack = json_tokener_parse(data);
    if(!haystack) {
        logger_log("%s %d: Failed to tokenize json!", __FILE__, __LINE__);
        goto error;
    }

    partition_name(haystack, tablename);
    buf = ht_search(m->htable, tablename, HT_UPSERT);

    // get value from json, apply transformation
    //
    // TODO: must use different format functions for text COPY format
    // (for timestamp in particular)
    if((ret = extract_needles_from_haystack(haystack, internal) != 0)) {
        if(ret == -1)
            logger_log("%s %d: Failed to dereference json!\n %s",
                __FILE__, __LINE__, data);
        for (int i = 0; i < internal->ncount; i++) {
            needles[i]->free(&needles[i]->result);
        }
        goto fail;
    }

    for (int i = 0; i < internal->ncount; i++)
    {
        const char *key = needles[i]->jpointer;

        if(!needles[i]->store)
            continue;

        if (i > 0)
            buffer_write(buf, "\t", 1);

        if(needles[i]->result)
            buffer_write(buf, needles[i]->result, needles[i]->length);
        else
            buffer_write(buf, "\\N", 2);

        // Jpointers usually start with slash. We need 'clear' key without
        // trailing symbols
        key = (*key == '/') ? key + 1 : key;

        // We only want to store as json those fields that we haven't extracted
        json_object_object_del(haystack, key);
    }

    // Write the rest of json as last column
    const char *json = json_object_to_json_string(haystack);
    uint32_t length = strlen(json);
    if (internal->ncount > 0)
            buffer_write(buf, "\t", 1);
    buffer_write(buf, json, length);
    buffer_write(buf, "\n", 1);

    m->count = m->count + 1;

    // TODO: flush based on stored data size in buffer
    if (m->count == 2000)
    {
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
    HTable     *ht = m->htable;
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

        // With the way iterator is implemented it's safe to remove items
        // as we iterate.
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
    config_setting_t   *setting = NULL;
    const char     *host = NULL;
    const char     *topic = NULL;

    if (!CONF_L_IS_STRING(config, "host", &host, "require host string!"))
        return false;

    if (!CONF_L_IS_STRING(config, "topic", &topic, "require topic string!"))
        return false;

    if (!(setting = CONF_GET_MEM(config,"jpointers", "require jpointers!")))
        return false;

    if (!validate_jpointers(setting))
        return false;

    return true;
}

Validator
bagger_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_consumer = &bagger_validate;
    v->validate_producer = &bagger_validate;
    return v;
}

static int
bagger_commit_worker(void *meta)
{
    BaggerMeta *m = (BaggerMeta *) meta;

#ifdef PR_SET_NAME
    prctl(PR_SET_NAME, "commit_worker");
#endif

    while(42)
    {
        HTable     *ht = m->htable;
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

             // With the way iterator is implemented it's safe to remove items
             // as we iterate.
            ht_search(ht, key, HT_REMOVE);
        }
        free(iter);

        mtx_unlock(&m->commit_mtx);
    }

    return 0;
}

