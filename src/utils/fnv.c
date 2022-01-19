 /*
 ***
 *
 * Please do not copyright this code.  This code is in the public domain.
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * By:
 *	chongo <Landon Curt Noll> /\oo/\
 *      http://www.isthe.com/chongo/
 *
 * Share and Enjoy!	:-)
 */

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

static inline Fnv32_t _fold(uint8_t fold, Fnv32_t hval)
{
    uint32_t mask = (1<<(uint32_t)fold) -1;
    return(hval>>(32-fold)) ^ (hval & mask);
}

Fnv32_t fold_noop(Fnv32_t hval) {return hval;}
Fnv32_t fold31(Fnv32_t hval) {return _fold(31,hval);}
Fnv32_t fold30(Fnv32_t hval) {return _fold(30,hval);}
Fnv32_t fold29(Fnv32_t hval) {return _fold(29,hval);}
Fnv32_t fold28(Fnv32_t hval) {return _fold(28,hval);}
Fnv32_t fold27(Fnv32_t hval) {return _fold(27,hval);}
Fnv32_t fold26(Fnv32_t hval) {return _fold(26,hval);}
Fnv32_t fold25(Fnv32_t hval) {return _fold(25,hval);}
Fnv32_t fold24(Fnv32_t hval) {return _fold(24,hval);}
Fnv32_t fold23(Fnv32_t hval) {return _fold(23,hval);}
Fnv32_t fold22(Fnv32_t hval) {return _fold(22,hval);}
Fnv32_t fold21(Fnv32_t hval) {return _fold(21,hval);}
Fnv32_t fold20(Fnv32_t hval) {return _fold(20,hval);}
Fnv32_t fold19(Fnv32_t hval) {return _fold(19,hval);}
Fnv32_t fold18(Fnv32_t hval) {return _fold(18,hval);}
Fnv32_t fold17(Fnv32_t hval) {return _fold(17,hval);}
Fnv32_t fold16(Fnv32_t hval) {return _fold(16,hval);}
Fnv32_t fold15(Fnv32_t hval) {return _fold(15,hval);}
Fnv32_t fold14(Fnv32_t hval) {return _fold(14,hval);}
Fnv32_t fold13(Fnv32_t hval) {return _fold(13,hval);}
Fnv32_t fold12(Fnv32_t hval) {return _fold(12,hval);}
Fnv32_t fold11(Fnv32_t hval) {return _fold(11,hval);}
Fnv32_t fold10(Fnv32_t hval) {return _fold(10,hval);}
Fnv32_t fold9(Fnv32_t hval) {return _fold(9,hval);}
Fnv32_t fold8(Fnv32_t hval) {return _fold(8,hval);}
Fnv32_t fold7(Fnv32_t hval) {return _fold(7,hval);}
Fnv32_t fold6(Fnv32_t hval) {return _fold(6,hval);}
Fnv32_t fold5(Fnv32_t hval) {return _fold(5,hval);}
Fnv32_t fold4(Fnv32_t hval) {return _fold(4,hval);}
Fnv32_t fold3(Fnv32_t hval) {return _fold(3,hval);}
Fnv32_t fold2(Fnv32_t hval) {return _fold(2,hval);}
Fnv32_t fold1(Fnv32_t hval) {return _fold(1,hval);}

typedef enum {
    FNV_UNDEF,
    FNV32A_STR,
    FNV32A_INT
} FnvTypes;

typedef enum {
    FOLD_UNDEF,
    FOLD_NOOP,
    FOLD31,
    FOLD30,
    FOLD29,
    FOLD28,
    FOLD27,
    FOLD26,
    FOLD25,
    FOLD24,
    FOLD23,
    FOLD22,
    FOLD21,
    FOLD20,
    FOLD19,
    FOLD18,
    FOLD17,
    FOLD16,
    FOLD15,
    FOLD14,
    FOLD13,
    FOLD12,
    FOLD11,
    FOLD10,
    FOLD9,
    FOLD8,
    FOLD7,
    FOLD6,
    FOLD5,
    FOLD4,
    FOLD3,
    FOLD2,
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
        {FOLD31, "fold31", &fold31},
        {FOLD30, "fold30", &fold30},
        {FOLD29, "fold29", &fold29},
        {FOLD28, "fold28", &fold28},
        {FOLD27, "fold27", &fold27},
        {FOLD26, "fold26", &fold26},
        {FOLD25, "fold25", &fold25},
        {FOLD24, "fold24", &fold24},
        {FOLD23, "fold23", &fold23},
        {FOLD22, "fold22", &fold22},
        {FOLD21, "fold21", &fold21},
        {FOLD20, "fold20", &fold20},
        {FOLD19, "fold19", &fold19},
        {FOLD18, "fold18", &fold18},
        {FOLD17, "fold17", &fold17},
        {FOLD16, "fold16", &fold16},
        {FOLD15, "fold15", &fold15},
        {FOLD14, "fold14", &fold14},
        {FOLD13, "fold13", &fold13},
        {FOLD12, "fold12", &fold12},
        {FOLD11, "fold11", &fold11},
        {FOLD10, "fold10", &fold10},
        {FOLD9, "fold9", &fold9},
        {FOLD8, "fold8", &fold8},
        {FOLD7, "fold7", &fold7},
        {FOLD6, "fold6", &fold6},
        {FOLD5, "fold5", &fold5},
        {FOLD4, "fold4", &fold4},
        {FOLD3, "fold3", &fold3},
        {FOLD2, "fold2", &fold2},
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
