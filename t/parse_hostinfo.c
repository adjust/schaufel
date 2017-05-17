#include <test/test.h>
#include <utils/helper.h>

int
main(void)
{
    char *hostinfo = "127.0.0.1:5432,127.0.0.1:5433,127.0.0.1:5434";

    Array a = parse_hostinfo(hostinfo);

    char *result_a = array_get(a, 0);
    char *result_b = array_get(a, 1);
    char *result_c = array_get(a, 2);

    pretty_assert(strncmp("127.0.0.1:5432", result_a, 14) == 0);
    pretty_assert(strncmp("127.0.0.1:5433", result_b, 14) == 0);
    pretty_assert(strncmp("127.0.0.1:5434", result_c, 14) == 0);

    array_free(&a);
    return 0;
}
