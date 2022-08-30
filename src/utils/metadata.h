#ifndef _SCHAUFEL_UTILS_METADATA_H
#define _SCHAUFEL_UTILS_METADATA_H

#include "../schaufel.h"
#include "utils/htable.h"
#include <stdint.h>


#define MAXELEM 8

typedef enum {
    MTYPE_STRING,
    MTYPE_INT,
    MTYPE_BIGINT,
    MTYPE_FUNC,
    MTYPE_OPAQUE
} MTypes;

/* we need a union here to have ISO C compatible
 * function pointers */
typedef struct Message *Message;
typedef union datum {
    uint32_t  *value;
    char      *string;
    bool     (*func) (Message);
    void      *ptr;
} Datum;

typedef struct mdatum {
    HTableNode node;
    char      *key;
    Datum      value;
    uint64_t   len;
    MTypes     type;
} *MDatum;


/* alias htable pointer as Metadata */
typedef struct HTable *Metadata;

MDatum metadata_find(Metadata *m, char *key);
MDatum metadata_insert(Metadata *m, char *key, MDatum datum);
MDatum mdatum_init(MTypes type, Datum value, uint64_t len);
bool metadata_callback_run(Metadata *m, Message msg);
void metadata_free(Metadata *m);

#endif
