#ifndef _SCHAUFEL_UTILS_POSTGRES_H
#define _SCHAUFEL_UTILS_POSTGRES_H

#include <libpq-fe.h>
#include <pthread.h>
#include <json-c/json.h>


typedef enum {
    PQ_COPY_TEXT,
    PQ_COPY_CSV,
    PQ_COPY_BINARY
} PqCopyFormat;

typedef struct Internal *Internal;

typedef struct Meta {
    PGconn          *conn_master;
    PGconn          *conn_replica;
    PGresult        *res;
    char            *conninfo;
    char            *conninfo_replica;
    char            *cpycmd;
    int             cpyfmt;
    int             count;
    int             copy;
    int             commit_iter;
    pthread_mutex_t commit_mutex;
    pthread_t       commit_worker;
    Internal        internal;
} *Meta;

void commit(Meta *m);

void *commit_worker(void *meta);
char *json_to_pqtext_esc(json_object *obj, size_t *len);
#endif
