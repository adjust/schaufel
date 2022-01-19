#include "schaufel.h"
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "utils/metadata.h"
#include "utils/scalloc.h"

/* todo: hsearch_r (etc.pp.) are GNUisms
 * we should be able to fall back to a standalone implementation
 * or use bintrees */

void mdatum_free(HTableNode *, void *);
MDatum mdatum_init(MTypes type, void *value, uint64_t len);

static void*
alloc_func(size_t size, void *arg)
{
    return SCALLOC(1,size);
}

static bool
keyeq_func(const HTableNode* a_, const HTableNode* b_, void *arg)
{
    MDatum a = (MDatum)a_;
    MDatum b = (MDatum)b_;
    return (strcmp(a->key, b->key) == 0);
}

static uint32_t
hash_func(const HTableNode *a_, void *arg)
{
    MDatum a = (MDatum)a_;
    return htable_default_hash(a->key, sizeof(a->key));
}

static void
free_func(void* mem, void *arg)
{
    free(mem);
}

static inline Metadata
_metadata_init()
{
    Metadata m = SCALLOC(1,sizeof(*m));

    // next line makes no sense anylonger
//    m->mdata = SCALLOC(8,sizeof(*(m->mdata)));
    m->htab = SCALLOC(1,sizeof(*(m->htab)));

    htable_create(
            m->htab,
            sizeof(struct mdatum),
            hash_func,
            keyeq_func,
            alloc_func,
            free_func,
            mdatum_free,
            NULL
    );

    return m;
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
    MDatum ret = (MDatum) htable_find(m->htab, (HTableNode *) &query);
    if(!ret) return NULL;

    return ret;
}

/*
 * mdatum_init
 *      initialise metadata value
 */
MDatum
mdatum_init(MTypes type, void *value, uint64_t len)
{
    MDatum datum = SCALLOC(1,sizeof(*datum));
    datum->type = type;
    datum->value = value;
    datum->len = len;
    return datum;
}

/*
 * mdatum_free
 *      free metadatum
 */
void
mdatum_free(HTableNode* n, void *arg)
{
    MDatum m = (MDatum) n;

    if(m->type == MTYPE_STRING)
        free(m->value);
    return;
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
    htable_insert(m->htab, (HTableNode *) value, &isNewNode);
    MDatum ret = (MDatum) htable_find(m->htab, (HTableNode *) value);
    free(value);

    // key already exists
    if (!isNewNode) goto error;

    return ret;
    error:
    return NULL;
}

void
metadata_free(Metadata *md)
{
    if(md == NULL)
        return;
    Metadata m = *md;
    if(m == NULL)
        return;
    htable_free_items(m->htab);
    free(m->htab);
    free(m);

    md = NULL;
    return;
}
