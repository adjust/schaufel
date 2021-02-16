#include <string.h>
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
 *      Stringify json object and escape all special characters. Returns
 *      allocated string and writes string length to `len`. Always use
 *      free_pqtext_esc() to deallocate the result as it requires special
 *      treatment.
 */
char *
json_to_pqtext_esc(PGconn *conn, json_object *obj, size_t *len)
{
    const char *json;
    char *esc;
    char *res;

    json = json_object_to_json_string(obj);
    esc = PQescapeLiteral(conn, json, strlen(json));

    if (!esc) {
        logger_log("%s %d: %s", __FILE__, __LINE__, PQerrorMessage(conn));
        abort();
    }
    // remove leading/trailing quotes etc. added by PQescapeLiteral
    res = esc;
    if (esc[0] == ' ' && esc[1] == 'E') {
        res = esc + 3;
        esc[strlen(esc) - 1] = '\0';
    } else if (esc[0] == '\'') {
        res = esc + 1;
        esc[strlen(esc) - 1] = '\0';
    }
    *len = strlen(res);

    // We return not the same pointer that was allocated by PQescapeLiteral.
    // Therefore we must do the reverse conversion when we deallocate the
    // string.
    return res;
}

void
free_pqtext_esc(void **obj)
{
    char *esc;
    char *ptr;

    // this function is supposed to be idempotent
    if(!obj)
        return;
    if(!*obj)
        return;
    esc = *obj;

    // see json_to_pqtext_esc for details
    if (*(esc - 1) != '\'') {
        logger_log("%s %d: unexpected byte", __FILE__, __LINE__);
        abort();
    }
    if (*(esc - 2) == 'E') {
        ptr = esc - 3;
    } else if (*(esc - 2) == ' ') {
        ptr = esc - 1;
    } else {
        logger_log("%s %d: unexpected byte", __FILE__, __LINE__);
        abort();
    }

    PQfreemem(ptr);
    *obj = NULL;
}
