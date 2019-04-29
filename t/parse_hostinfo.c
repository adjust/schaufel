#include "test/test.h"
#include "utils/array.h"
#include "utils/helper.h"


int
main(void)
{
    char *hostinfo = "127.0.0.1:5432,127.0.0.1:5433,127.0.0.1:5434;127.0.0.1:5435,127.0.0.1:5436,127.0.0.1:5437";

    Array a = parse_hostinfo_master(hostinfo);

    char *result_a = array_get(a, 0);
    char *result_b = array_get(a, 1);
    char *result_c = array_get(a, 2);

    pretty_assert(strncmp("127.0.0.1:5432", result_a, 14) == 0);
    pretty_assert(strncmp("127.0.0.1:5433", result_b, 14) == 0);
    pretty_assert(strncmp("127.0.0.1:5434", result_c, 14) == 0);

    Array b = parse_hostinfo_replica(hostinfo);

    char *result_d = array_get(b, 0);
    char *result_e = array_get(b, 1);
    char *result_f = array_get(b, 2);

    pretty_assert(strncmp("127.0.0.1:5435", result_d, 14) == 0);
    pretty_assert(strncmp("127.0.0.1:5436", result_e, 14) == 0);
    pretty_assert(strncmp("127.0.0.1:5437", result_f, 14) == 0);

    array_free(&a);
    array_free(&b);
    return 0;
}
