#include <search.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "utils/scalloc.h"
#include "utils/metadata.h"

/* todo: hsearch_r (etc.pp.) are GNUisms
 * we should be able to fall back to a standalone implementation
 * or use bintrees */

static inline Metadata
_metadata_init()
{
    Metadata m = SCALLOC(1, sizeof(*m));
    m->entries = NULL;
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

    mdatum_hash_entry *entry;
    HASH_FIND(hh, m->entries, key, strlen(key), entry);
    if (entry)
        return entry->datum;
    else
        return NULL;
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
    mdatum_hash_entry *entry = SCALLOC(1, sizeof(mdatum_hash_entry));

    if(value == NULL)
        goto error;

    if(m == NULL) {
        m = _metadata_init();
        *md = m;
    }
    else if ((m->nel)+1 > MAXELEM)
        goto error;

    entry->key = key;
    entry->datum = value;

    HASH_ADD_KEYPTR(hh, m->entries, key, strlen(key), entry);
    m->nel++;

    return value;

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

    mdatum_hash_entry *entry, *tmp_entry;
    HASH_ITER(hh, m->entries, entry, tmp_entry)
    {
        free(entry->datum->value);
        free(entry->datum);
        HASH_DEL(m->entries, entry);
        free(entry);
    }
    HASH_CLEAR(hh, m->entries);
    free(m);

    md = NULL;
    return;
}
