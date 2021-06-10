#include "test/test.h"
#include "utils/strlwr.h"
#include <stdlib.h>

int main()
{
    char hurz[13] = "Der Habicht!\0";
    strlwr(hurz);
    pretty_assert(strcmp(hurz, "der habicht!") == 0);

    char *foo = strlwr(strdup("BAR"));
    pretty_assert(strcmp(foo, "bar") == 0);
    free(foo);
    return 0;
}
