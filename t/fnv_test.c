#include "test/test.h"
#include "utils/fnv.h"

int main()
{
    Fnv32_t (*fnv)(void *,size_t) = fnv_init("fnv32a_str");
    Fnv32_t (*fold16)(Fnv32_t) = fold_init("fold16");
    Fnv32_t (*fold1)(Fnv32_t) = fold_init("fold1");
    Fnv32_t (*fold)(Fnv32_t) = fold_init("fold_noop");
    Fnv32_t res = 0;
    pretty_assert(fnv !=  NULL);
    pretty_assert(fold16 !=  NULL);
    pretty_assert(fold1 !=  NULL);

    // hurz hashes to 0x60bdfa92 (in fnv-1a 32 bit)
    res = fnv("hurz",strlen("hurz"));
    pretty_assert(0x60bdfa92 == res);

    res = fold16((Fnv32_t) 0x60bdfa92);
    pretty_assert(res == 0x9a2f);
    res = fold1((Fnv32_t) 0x60bdfa92);
    pretty_assert(res == 0);
    res = fold((Fnv32_t) 0x60bdfa92);
    pretty_assert(res == 0x60bdfa92);

    return 0;
}
