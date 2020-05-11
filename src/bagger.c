#include <ctype.h>
#include <errno.h>
#include <libpq-fe.h>
#include <threads.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>

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
#define BUFFER_SIZE_THRESHOLD (8096 * 8)
#define BUFFER_EPOCH_THRESHOLD 12
#define BUFFER_NODE_SIZE 4096
#define COMMIT_WORKER_DELAY 5


typedef struct {
    PGconn         *conn;
    char           *hostname;
    int             port;
} Conn;

typedef struct BaggerMeta
{
    mtx_t           commit_mtx;
    thrd_t          commit_thrd;
    _Atomic bool    commit_thrd_exit;
    Internal        internal;
    HTable         *htable; // hashtable of per-table buffers

    // connection parameters
    Conn            master, replica;
    const char     *dbname;
    const char     *user;
    const char     *generation;
} BaggerMeta;

typedef struct BufferNode BufferNode;

typedef struct Buffer {
    BufferNode     *buf_head;   // buffers list
    BufferNode     *buf_tail;   // last buffers list element
    size_t          nbytes;     // bytes actually written
    int             epoch;
} Buffer;

typedef struct BufferNode {
    char            data[BUFFER_NODE_SIZE];
    BufferNode     *next;
} BufferNode;

static void bagger_producer_free(Producer *p);
static void bagger_producer_produce(Producer p, Message msg);
static void bagger_consumer_free(Consumer *c);
static int bagger_consumer_consume(Consumer c, Message msg);

static char *_cpycmd(const BaggerMeta *m, const char *table);
static int bagger_commit_worker(void *);

static void
buffer_write(Buffer *buf, const char *data, size_t len)
{
    BufferNode *node = buf->buf_tail;

    while (len > 0) {
        // Calculate remaining space in the buffer node
        size_t  offset = buf->nbytes % BUFFER_NODE_SIZE;
        size_t  bytes_left = BUFFER_NODE_SIZE - offset;

        // Do we need to allocate a new node?
        if (offset == 0)
        {
            BufferNode *new_node = SCALLOC(1, sizeof(BufferNode));

            // If this is the first node make it the head of the list.
            // Otherwise attach it to the end.
            if (!node)
                buf->buf_head = new_node;
            else
                node->next = new_node;
            buf->buf_tail = new_node;
            node = new_node;

            bytes_left = BUFFER_NODE_SIZE;
        }

        size_t  bytes_copied = min(bytes_left, len);
        memcpy(node->data + offset, data, bytes_copied);

        data += bytes_copied;
        len -= bytes_copied;
        buf->nbytes += bytes_copied;
    };

    // Reset epoch
    buf->epoch = 0;
}

static void
buffer_flush(BaggerMeta *m, Buffer *buf, const char *tablename)
{
    Conn       *servers[] = {&m->master, &m->replica};
    int         nservers;
    char       *cpycmd;
    size_t      nbytes;
    BufferNode *node,
               *prev;

    // ignore replica if it wasn't specified
    nservers = m->replica.hostname ? 2 : 1;

    cpycmd = _cpycmd(m, tablename);
    for (int i = 0; i < nservers; ++i) {
        PGresult *res = PQexec(servers[i]->conn, cpycmd);

        if (PQresultStatus(res) != PGRES_COPY_IN) {
            logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(servers[i]->conn));
            abort();
        }
    }
    free(cpycmd);

    nbytes = buf->nbytes;

    // write out buffers
    node = buf->buf_head;
    while (node != NULL)
    {
        size_t bufsize = min(nbytes, BUFFER_NODE_SIZE);

        nbytes -= bufsize;
        for (int i = 0; i < nservers; ++i) {
            if (PQputCopyData(servers[i]->conn, node->data, bufsize) < 0) {
                logger_log("%s %d: PQputCopyData failed: %s",
                           __FILE__, __LINE__, PQerrorMessage(servers[i]->conn));
                abort();
            }
        }
        // pg_put_copy_data(m, node->data, bufsize);

        prev = node;
        node = node->next;

        free(prev);
    }

    // reinitialize buffer for further use
    buf->buf_head = NULL;
    buf->buf_tail = NULL;
    buf->nbytes = 0;

    // finish write
    for (int i = 0; i < nservers; ++i)
    {
        if (PQputCopyData(servers[i]->conn, "\\.\n", 3) < 0) {
            logger_log("%s %d: PQputCopyData failed: %s",
                       __FILE__, __LINE__, PQerrorMessage(servers[i]->conn));
            abort();
        }
        if (PQputCopyEnd(servers[i]->conn, NULL) < 0) {
            logger_log("%s %d: PQputCopyEnd failed: %s",
                       __FILE__, __LINE__, PQerrorMessage(servers[i]->conn));
            abort();
        }
    }
}

static char *
_connectinfo(const char *host, int port, const char *dbname, const char *user)
{
    const char *fmtstr = "dbname=%s user=%s host=%s port=%d";
    char   *conninfo;
    size_t  len;

    len = snprintf(NULL, 0, fmtstr, dbname, user, host, port);
    conninfo = (char *) SCALLOC(1, len);
    sprintf(conninfo, fmtstr, dbname, user, host, port);

    return conninfo;
}

static char *
_cpycmd(const BaggerMeta *m, const char *table)
{
    const char *fmtstr = "COPY %s_%d_%s.%s FROM STDIN (FORMAT text)";
    size_t  len;
    char   *cpycmd;

    len = snprintf(NULL, 0, fmtstr, m->master.hostname, m->master.port,
                   m->generation, table);
    cpycmd = (char *) SCALLOC(1, len);
    sprintf(cpycmd, fmtstr, m->master.hostname, m->master.port,
            m->generation, table);

    return cpycmd;
}

static uint32_t
hashfunc(const char *key)
{
    return MurmurHash2(key, strlen(key), 0);
}

BaggerMeta *
bagger_meta_init(const char *host, const char *replica, const char *dbname,
                 const char *user, const char *topic,
                 config_setting_t *needlestack)
{
    BaggerMeta     *m = SCALLOC(1, sizeof(*m));
    Internal        i = SCALLOC(1, sizeof(*i));
    char           *conninfo;

    m->internal = i;
    m->internal->needles = transform_needles(needlestack, i, PQ_COPY_TEXT);
    m->internal->ncount = config_setting_length(needlestack);
    m->htable = ht_create(16, sizeof(Buffer), hashfunc);
    m->generation = topic;
    m->dbname = dbname;
    m->user = user;

    // parse host string and establish connection to master
    if (parse_connstring(host, &m->master.hostname, &m->master.port) == -1) {
        logger_log("%s %d: failed to parse host string", __FILE__, __LINE__ );
        abort();
    }

    conninfo = _connectinfo(m->master.hostname, m->master.port, m->dbname, m->user);
    m->master.conn = PQconnectdb(conninfo);
    if (PQstatus(m->master.conn) != CONNECTION_OK)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->master.conn));
        abort();
    }
    free(conninfo);

    // establish connection to replica if needed
    if (replica) {
        if (parse_connstring(replica, &m->replica.hostname, &m->replica.port) == -1) {
            logger_log("%s %d: failed to parse replica host", __FILE__, __LINE__ );
            abort();
        }

        conninfo = _connectinfo(m->replica.hostname, m->replica.port, m->dbname, m->user);
        m->replica.conn = PQconnectdb(conninfo);
        if (PQstatus(m->replica.conn) != CONNECTION_OK)
        {
            logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(m->replica.conn));
            abort();
        }
        free(conninfo);
    }

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

    PQfinish(m->master.conn);
    if (m->replica.hostname)
        PQfinish(m->replica.conn);

    for (int i = 0; i < internal->ncount; i++ ) {
        free(internal->needles[i]->jpointer);
        internal->needles[i]->free(
            &(internal->needles[i]->result));
        free(internal->needles[i]);
    }
    free(internal->needles);
    free(internal->leapyears);
    free(internal);
    free(m->htable);
    free(m->master.hostname);
    if (m->replica.hostname)
        free(m->replica.hostname);

    free(m);
}

Producer
bagger_producer_init(config_setting_t *config)
{
    const char     *host = NULL,
                   *replica = NULL,
                   *topic = NULL,
                   *user = NULL,
                   *dbname = NULL;
    config_setting_t *needlestack = NULL;

    // config defaults
    config_set_default_string(config, "user", "postgres");
    config_set_default_string(config, "dbname", "postgres");

    // read config
    config_setting_lookup_string(config, "host", &host);
    config_setting_lookup_string(config, "replica", &replica);
    config_setting_lookup_string(config, "dbname", &dbname);
    config_setting_lookup_string(config, "user", &user);
    config_setting_lookup_string(config, "topic", &topic);
    needlestack = config_setting_get_member(config, "jpointers");

    Producer bagger = SCALLOC(1, sizeof(*bagger));
    bagger->meta          = bagger_meta_init(host, replica, dbname, user,
                                             topic, needlestack);
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
    char            ts[14];

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

    // As timestamp we get a string like "YYYY-MM-DD hh:mm:ss". In bagger each
    // partition contains records for one hour timespan. Hence we need to
    // transform the original timestamp to "YYYY_MM_DD_hh" which would comply
    // with both hourly principle and postgres naming rules.
    for (i = 0; i < 13 && values[2][i] != '\0'; i++)
        ts[i] = isdigit(values[2][i]) ? values[2][i] : '_';
    ts[i] = '\0';

    snprintf(out, 64, "data_%s_%s_%s", values[0], values[1], ts);
}

// Delete a nested key
static void
json_remove_key(json_object *json, char *path)
{
    int     ret;
    char   *key;
    char   *delim;
    json_object *obj = json;

    delim = strrchr(path, '/');
    key = delim + 1;

    // apparently not a proper path
    if (delim == NULL)
        return;

    // if it's a non-trivial path extract the json object containing the key
    if (delim != path)
    {
        // temporarily replace last slash in the path with terminal zero
        *delim = '\0';

        ret = json_pointer_get(json, path, &obj);
        if (ret < 0 || obj == NULL)
            return;

        // put the delimiter back
        *delim = '/';
    }

    json_object_object_del(obj, key);
}

// json_stringify
//      Stringify json object and escape all backslash characters. Returns
//      allocated string and writes string length to `len`.
static char *
json_stringify(json_object *obj, size_t *len)
{
    const char *json = json_object_to_json_string(obj);
    const char *j = json;
    char       *res;
    char       *r;
    size_t      esc_num = 0;

    *len = 0;

    // count escape characters in order to allocate large enough string for
    // result
    while (*j)
    {
        esc_num += (*j == '\\') ? 1 : 0;
        (*len)++;
        j++;
    }
    *len += esc_num;

    res = SCALLOC(1, *len);

    j = json;
    r = res;
    while (*j)
    {
        // escape backslash
        if (*j == '\\')
            *r++ = '\\';

        *r++ = *j++;
    }

    return res;
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
        char *key = needles[i]->jpointer;

        if(!needles[i]->store)
            continue;

        if (i > 0)
            buffer_write(buf, "\t", 1);

        if(needles[i]->result)
            buffer_write(buf, needles[i]->result, needles[i]->length);
        else
            buffer_write(buf, "\\N", 2);

        json_remove_key(haystack, key);
    }

    // Write the rest of json as last column
    size_t length;
    char *json = json_stringify(haystack, &length);
    if (internal->ncount > 0)
            buffer_write(buf, "\t", 1);
    buffer_write(buf, json, length);
    buffer_write(buf, "\n", 1);
    SFREE(json);

    // Flush when buffer size exceeds the threshold. If it doesn't then
    // it will be eventually flushed by bagger_commit_worker.
    if (buf->nbytes >= BUFFER_SIZE_THRESHOLD) {
        buffer_flush(m, buf, tablename);

        ht_search(m->htable, tablename, HT_REMOVE);
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

    bagger->meta          = bagger_meta_init(host, NULL, NULL, NULL, NULL, needles);
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

    if (!validate_jpointers(setting, PQ_COPY_TEXT))
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

        for (int i = 0; i < COMMIT_WORKER_DELAY; ++i)
        {
            sleep(1);
            if (m->commit_thrd_exit)
                return 0;
        }

        mtx_lock(&m->commit_mtx);

        iter = ht_iter_create(ht);
        while ((key = ht_iter_next(iter, (void **) &buf)) != NULL)
        {
            // Simple technique to prevent half empty buffers being flushed
            // too early. This is particularly useful when postgres table is
            // created using pg_cryogen (or similar storage) which works best
            // with large batches of data.
            if (buf->epoch < BUFFER_EPOCH_THRESHOLD)
            {
                buf->epoch++;
                continue;
            }

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

