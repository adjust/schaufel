#include "schaufel.h"
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "helper.h"
#include "utils/metadata.h"
#include "utils/scalloc.h"

/* todo: hsearch_r (etc.pp.) are GNUisms
 * we should be able to fall back to a standalone implementation
 * or use bintrees */

/*
 * mdatum_free
 *      free metadatum
 *      required as free function for hashtable
 */
void
mdatum_free(HTableNode* n, UNUSED void *arg)
{
    MDatum m = (MDatum) n;

    // todo: is it good to run callbacks on free?
    if(m->type != MTYPE_FUNC)
        free(m->value.ptr);
    else // disown function pointer
        m->value.func = NULL;
    return;
}


bool
metadata_callback_run(Metadata *md, Message msg)
{
    MDatum m = metadata_find(md, "callback");
    if(m == NULL) return true;

    // This should throw an error
    if(m->type != MTYPE_FUNC || m->value.func == NULL)
        return false;

    return m->value.func(msg);
}

/*
 * _alloc_func
 *      allocate internal copy of hashtable entry
 */
static void*
_alloc_func(size_t size, UNUSED void *arg)
{
    return SCALLOC(1,size);
}

/*
 * _keyeq_func
 *      hashtable function to prove key equality
 */
static bool
_keyeq_func(const HTableNode* a_, const HTableNode* b_, UNUSED void *arg)
{
    MDatum a = (MDatum)a_;
    MDatum b = (MDatum)b_;
    return (strcmp(a->key, b->key) == 0);
}

/*
 * _hash_func
 *      define what key to hash on
 */
static uint32_t
_hash_func(const HTableNode *a_, UNUSED void *arg)
{
    MDatum a = (MDatum)a_;
    return htable_default_hash(a->key, strlen(a->key));
}

/*
 * _free_func
 *      free internal copy of metadatum in hashtable
 */
static void
_free_func(void* mem, UNUSED void *arg)
{
    free(mem);
}

/*
 * _metadata_init
 *      initialize hashtable
 */

static inline Metadata
_metadata_init()
{
    HTable *m = SCALLOC(1,sizeof(*m));
    htable_create(
            m,
            sizeof(struct mdatum),
            _hash_func,
            _keyeq_func,
            _alloc_func,
            _free_func,
            mdatum_free,
            NULL
    );

    return (Metadata) m;
}

/*
 * metadata_find
 *      find metadatum in metadata
 */
MDatum
metadata_find(Metadata *md, char *key)
{
    Metadata m = *md;

    if(m == NULL)
        return NULL;
    struct mdatum query;
    query.key = key;
    MDatum ret = (MDatum) htable_find((HTable *) m, (HTableNode *) &query);
    if(!ret) return NULL;

    return ret;
}

/*
 * mdatum_init
 *      initialise metadata value
 */
MDatum
mdatum_init(MTypes type, Datum value, uint64_t len)
{
    MDatum datum = SCALLOC(1,sizeof(*datum));
    datum->type = type;
    datum->value = value;
    datum->len = len;
    return datum;
}


/*
 * metadata_insert
 *      insert key/value into metadata
 *      initialise metadata if it doesn't yet exist
 *      if entry already exists, return NULL
 */
MDatum
metadata_insert(Metadata *md, char *key, MDatum value)
{
    Metadata m = *md;
    if(value == NULL)
        goto error;

    if(m == NULL) {
        m = _metadata_init();
        *md = m;
    }
    bool isNewNode;
    value->key = key;
    htable_insert((HTable *) m, (HTableNode *) value, &isNewNode);
    MDatum ret = (MDatum) htable_find((HTable *)m, (HTableNode *) value);

    // the hashtable creates an internal copy of MDatum
    free(value);

    // key already exists
    if (!isNewNode) goto error;

    return ret;
    error:
    return NULL;
}

/*
 * metadata_free
 *      destroy hashtable
 */
void
metadata_free(Metadata *md)
{
    if(md == NULL)
        return;
    Metadata m = *md;
    if(m == NULL)
        return;
    htable_free_items((HTable *)m);
    free(m);

    md = NULL;
    return;
}
