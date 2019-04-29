#include <arpa/inet.h>
#include <unistd.h>

#include "utils/logger.h"
#include "utils/postgres.h"


void
commit(Meta *m)
{
    if((*m)->cpyfmt == PQ_COPY_BINARY)
        PQputCopyData((*m)->conn_master, "\377\377", 2);
    PQputCopyEnd((*m)->conn_master, NULL);
    if ((*m)->conninfo_replica)
        PQputCopyEnd((*m)->conn_replica, NULL);

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

void *
commit_worker(void *meta)
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
            commit(m);
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&((*m)->commit_mutex));
        pthread_testcancel();
    }
}
