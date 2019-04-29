#include "test/test.h"
#include "utils/array.h"


int
main(void)
{
    Array a = array_init(1);

    char *string_a = "moep";
    char *string_b = "huch";

    array_insert(a, string_a);
    array_insert(a, string_b);

    char *result_a = array_pop(a);
    char *result_b = array_pop(a);

    pretty_assert(strncmp("huch", result_a, 4) == 0);
    pretty_assert(strncmp("moep", result_b, 4) == 0);

    array_free(&a);
    return 0;
}
