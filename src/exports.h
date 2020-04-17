#ifndef _SCHAUFEL_EXPORTS_H_
#define _SCHAUFEL_EXPORTS_H_

#include <libconfig.h>
#include <json-c/json.h>

#include "producer.h"
#include "consumer.h"
#include "validator.h"
#include "queue.h"


typedef struct Needles *Needles;
typedef struct Needles {
    char           *jpointer;
    bool          (*format) (json_object *, Needles);
    void          (*free) (void **);
    uint32_t       *leapyears;
    uint32_t        length;
    void           *result; // output
    bool          (*action) (bool, json_object *, Needles);
    bool          (*filter) (bool, json_object *, Needles);
    bool            store;
    const char     *filter_data;
} *Needles;

typedef struct Internal {
    Needles        *needles;
    uint32_t       *leapyears;  // leapyears are globally shared
    uint16_t        ncount; // count of needles
    uint16_t        rows; // number of rows inserted into postgres
} *Internal;


Producer exports_producer_init(config_setting_t *config);
Consumer exports_consumer_init(config_setting_t *config);
Validator exports_validator_init();
bool validate_jpointers(config_setting_t *setting);

#if 0
typedef struct {
    const char *action_type;
    bool  (*action) (bool, json_object *, Needles);
    bool store;
} ExportsAction;

typedef struct {
    const char *filter_type;
    bool (*filter) (bool, json_object *, Needles);
    bool needs_data;
} ExportsFilter;

typedef struct {
    const char *pq_type;
    bool  (*format) (json_object *, Needles);
    void  (*free) (void **obj);
} PqType;

const ExportsAction *lookup_actiontype(const char *action_type);
const ExportsFilter *lookup_filtertype(const char *filter_type);
const PqType *lookup_pqtype(const char *pqtype);
#endif

Needles *transform_needles(config_setting_t *needlestack, Internal internal);
int extract_needles_from_haystack(json_object *haystack, Internal internal);

#endif
