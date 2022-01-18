#include "schaufel.h"
#include <stdlib.h>
#include <stdio.h>

#include "test/test.h"
#include "utils/helper.h"


int
main(void)
{
    char *conninfo = calloc(15, sizeof(*conninfo));
    char *hostname = NULL;

    int port, res;

    snprintf(conninfo, 15, "localhost:7432");
    res = parse_connstring(conninfo, &hostname, &port);

    pretty_assert(strncmp("localhost", hostname, 9) == 0);
    pretty_assert(port == 7432);
    pretty_assert(res  == 0);
    free(conninfo);
    free(hostname);
    port = 0;

    conninfo = calloc(14, sizeof(*conninfo));
    snprintf(conninfo, 14, "/tmp/123.sock");
    res = parse_connstring(conninfo, &hostname, &port);
    pretty_assert(strncmp("/tmp/123.sock", hostname, 13) == 0);
    pretty_assert(port == 0);
    pretty_assert(res  == 1);

    free(conninfo);
    free(hostname);
    port = 0;

    conninfo = calloc(15, sizeof(*conninfo));
    snprintf(conninfo, 15, "localhost:moep");
    res = parse_connstring(conninfo, &hostname, &port);

    pretty_assert(strncmp("localhost", hostname, 9) == 0);
    pretty_assert(port == 0);
    pretty_assert(res  == -1);
    free(conninfo);
    free(hostname);
}
