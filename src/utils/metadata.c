#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "utils/metadata.h"
#include "utils/scalloc.h"

#include <search.h>

/* todo: hsearch_r (etc.pp.) are GNUisms
 * we should be able to fall back to a standalone implementation
 * or use bintrees */

static inline Metadata
_metadata_init()
{
    Metadata m = SCALLOC(1,sizeof(*m));

    m->mdata = SCALLOC(8,sizeof(*(m->mdata)));
    m->htab = SCALLOC(1,sizeof(*(m->htab)));

    if(!hcreate_r(MAXELEM,(m->htab)))
        abort(); // todo: assert?

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
    ENTRY e, *ret;
    e.key = key;

    if(m == NULL)
        return NULL;

    if(hsearch_r(e,FIND,&ret,m->htab) == 0)
        return NULL;

    return (MDatum) ret->data;
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
 * metadata_insert
 *      insert key/value into metadata
 *      initialise metadata if it doesn't yet exist
 *      if entry already exists, return NULL
 */
MDatum
metadata_insert(Metadata *md, char *key, MDatum value)
{
    Metadata m = *md;
    ENTRY e, *retval;

    if(value == NULL)
        goto error;

    if(m == NULL) {
        m = _metadata_init();
        *md = m;
    }
    else if ((m->nel)+1 > MAXELEM)
        goto error;

    e.key = key;
    e.data = (void *) value;

    if((hsearch_r(e,ENTER,&retval,m->htab))== 0)
        goto error;

    *((m->mdata)+m->nel) = value;
    m->nel++;

    return (MDatum) retval->data;
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

    for(uint8_t i=0; i < m->nel; i++)
    {
        MDatum mdatum = *((m->mdata)+i);
        free(mdatum->value);
        free(mdatum);
    }

    free(m->mdata);
    hdestroy_r(m->htab);
    free(m->htab);
    free(m);

    md = NULL;
    return;
}
