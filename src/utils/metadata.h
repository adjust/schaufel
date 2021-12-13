#ifndef _SCHAUFEL_UTILS_METADATA_H
#define _SCHAUFEL_UTILS_METADATA_H

#include <stdint.h>
#include "uthash.h"

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

typedef struct mdatum_hash_entry {
    MDatum datum;
    char *key;
    UT_hash_handle hh;
} mdatum_hash_entry;

typedef struct Metadata {
    uint8_t nel;
    struct mdatum_hash_entry *entries;
} *Metadata;

MDatum metadata_find(Metadata *m, char *key);
MDatum metadata_insert(Metadata *m, char *key, MDatum datum);
MDatum mdatum_init(MTypes type, void *value, uint64_t len);

void metadata_free(Metadata *m);

#endif
