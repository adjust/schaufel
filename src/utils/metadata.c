#include <search.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "utils/scalloc.h"
#include "utils/metadata.h"

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

MDatum
mdatum_init(MTypes type, void *value, uint64_t len)
{
    MDatum datum = SCALLOC(1,sizeof(*datum));
    datum->type = type;
    datum->value = value;
    datum->len = len;
    return datum;
}

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
    else if ((m->nel)+1 >= MAXELEM)
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
