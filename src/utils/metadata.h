#ifndef _SCHAUFEL_UTILS_METADATA_H
#define _SCHAUFEL_UTILS_METADATA_H

#include "../schaufel.h"
#include <stdint.h>

#define MAXELEM 8

typedef enum {
    MTYPE_STRING,
    MTYPE_INT,
    MTYPE_BIGINT
} MTypes;

typedef struct mdatum {
    void    *value;
    uint64_t len;
    MTypes   type;
} *MDatum;

typedef struct Metadata {
    MDatum *mdata;
    uint8_t nel;
    struct hsearch_data *htab;
} *Metadata;

MDatum metadata_find(Metadata *m, char *key);
MDatum metadata_insert(Metadata *m, char *key, MDatum datum);
MDatum mdatum_init(MTypes type, void *value, uint64_t len);

void metadata_free(Metadata *m);

#endif
