#include <arpa/inet.h>
#include <json-c/json.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "utils/logger.h"
#include "utils/postgres.h"
#include "utils/scalloc.h"


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

/*
 * json_to_pqtext_esc
 *      Stringify json object and escape all backslash characters. Returns
 *      allocated string and writes string length to `len`.
 */
char *
json_to_pqtext_esc(json_object *obj, size_t *len)
{
    const char *json = json_object_to_json_string(obj);
    const char *j = json;
    char       *res;
    char       *r;
    size_t      esc_num = 0;

    *len = 0;

    /*
     * Count escape characters in order to allocate large enough string for
     * result
     */
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
        /* escape backslash */
        if (*j == '\\')
            *r++ = '\\';

        *r++ = *j++;
    }

    return res;
}
