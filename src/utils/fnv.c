#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "utils/fnv.h"
#include "utils/scalloc.h"
#include "utils/helper.h"

#define FNV_32A_INIT ((Fnv32_t)0x811c9dc5)
#define FNV_32_PRIME ((Fnv32_t)0x01000193)

static inline Fnv32_t _fnv32a(void *buf, size_t len, Fnv32_t hval)
{
    unsigned char *bp = (unsigned char *) buf;
    unsigned char *be = bp+len;

    while (bp < be)
    {
        hval ^= (Fnv32_t) *bp++;
        hval *= FNV_32_PRIME;
    }

    return hval;
}

Fnv32_t fnv32a_int(void *i, UNUSED size_t len)
{
    return _fnv32a(i, 4, FNV_32A_INIT);
}

Fnv32_t fnv32a_str(void *buf, size_t len)
{
    return _fnv32a((void *) buf, len, FNV_32A_INIT);
}

Fnv32_t fold_noop(Fnv32_t hval)
{
    return hval;
}

Fnv32_t fold16(Fnv32_t hval)
{
    #define MASK_16 (((uint32_t)1<<16)-1) /* i.e., (u_int32_t)0xffff */
    return (hval>>16) ^ (hval & MASK_16);
}

Fnv32_t fold1(Fnv32_t hval)
{
    #define MASK_1 (((uint32_t)1<<1)-1) /* i.e., (u_int32_t)0x1 */
    return (hval>>31) ^ (hval & MASK_1);
}

typedef enum {
    FNV_UNDEF,
    FNV32A_STR,
    FNV32A_INT
} FnvTypes;

typedef enum {
    FOLD_UNDEF,
    FOLD_NOOP,
    FOLD16,
    FOLD1,
} FoldTypes;

static const struct {
    FnvTypes type;
    const char *fnv_type;
    Fnv32_t  (*hash) (void *, size_t);
}  fnv_types [] = {
        {FNV_UNDEF, "undef", NULL},
        {FNV32A_STR, "fnv32a_str", &fnv32a_str},
        {FNV32A_INT, "fnv32a_int", &fnv32a_int},
};

static const struct {
    FoldTypes type;
    const char *fold_type;
    Fnv32_t  (*hash) (Fnv32_t);
}  fold_types [] = {
        {FOLD_UNDEF, "undef", NULL},
        {FOLD_NOOP, "fold_noop", &fold_noop},
        {FOLD16, "fold16", &fold16},
        {FOLD1, "fold1", &fold1},
};

static FnvTypes
_fnvtypes_enum(const char *fnv_type)
{
    uint32_t j;
    for (j = 1; j < sizeof(fnv_types) / sizeof(fnv_types[0]); j++)
        if (!strcmp(fnv_type, fnv_types[j].fnv_type))
            return fnv_types[j].type;
    return FNV_UNDEF;
}

static FoldTypes
_foldtypes_enum(const char *fold_type)
{
    uint32_t j;
    for (j = 1; j < sizeof(fold_types) / sizeof(fold_types[0]); j++)
        if (!strcmp(fold_type, fold_types[j].fold_type))
            return fold_types[j].type;
    return FOLD_UNDEF;
}

Fnv32_t (*fnv_init(char *name))(void *,size_t)
{
    uint32_t i = _fnvtypes_enum(name);
    return fnv_types[i].hash;
}


Fnv32_t (*fold_init(char *name))(Fnv32_t)
{
    uint32_t i = _foldtypes_enum(name);
    return fold_types[i].hash;
}
