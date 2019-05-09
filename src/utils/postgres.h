#ifndef _SCHAUFEL_UTILS_POSTGRES_H
#define _SCHAUFEL_UTILS_POSTGRES_H

#include <libpq-fe.h>
#include <pthread.h>

#define PQ_COPY_TEXT   0
#define PQ_COPY_CSV    1
#define PQ_COPY_BINARY 2

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
#endif
