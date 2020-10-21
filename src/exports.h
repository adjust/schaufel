#ifndef _SCHAUFEL_EXPORTS_H_
#define _SCHAUFEL_EXPORTS_H_

#include <libconfig.h>
#include <json-c/json.h>
#include <libpq-fe.h>

#include "producer.h"
#include "consumer.h"
#include "validator.h"
#include "queue.h"
#include "utils/postgres.h"


typedef struct Needles *Needles;
typedef struct Needles {
    char           *jpointer;
    bool          (*format) (PGconn *, json_object *, Needles);
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

bool validate_jpointers(config_setting_t *setting, PqCopyFormat fmt);
Needles *transform_needles(config_setting_t *needlestack, Internal internal,
                           PqCopyFormat fmt);
int extract_needles_from_haystack(PGconn *conn, json_object *haystack, Internal internal);

#endif
