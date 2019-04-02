#include <utils/helper.h>

size_t number_length(long number)
{
    size_t count = 0;
    if( number == 0 || number < 0 )
        ++count;
    while( number != 0 )
    {
        number /= 10;
        ++count;
    }
    return count;
}

int
parse_connstring(char *conninfo, char **hostname, int *port)
{
    const char *delim = ":";
    char *save,
         *port_str,
         *dup;
    int res = 0;

    dup = strdup(conninfo);
    *hostname = strdup(strtok_r(dup, delim, &save));
    if (*hostname == NULL)
        res = -1;
    else if ((port_str = strtok_r(NULL, delim, &save)) == NULL)
        res = 1;
    else if ((*port = atoi(port_str)) == 0)
        res = -1;

    free(dup);
    return res;
}

bool get_state(const volatile atomic_bool *state)
{
    return atomic_load(state);
}

bool set_state(volatile atomic_bool *state, bool value)
{
    bool expected = !value;

    return atomic_compare_exchange_strong(state, &expected, value);
}

Array
_delimit_by(char *str, char* delim)
{
    if (str == NULL)
        return NULL;
    Array a = array_init(1);
    char *match,
         *save,
         *dup,
         *dup_old;

    dup = strdup(str);
    dup_old = dup;

    while ((match = strtok_r(dup, delim, &save)) != NULL)
    {
        dup = NULL;
        array_insert(a, match);
    }

    if (dup != NULL)
        return NULL;

    free(dup_old);
    return a;
}

Array
parse_hostinfo_master(char *hostinfo)
{
    char *delim1 = ";";
    Array a = _delimit_by(hostinfo, delim1);
    char *delim = ",";
    Array b = _delimit_by(array_get(a, 0), delim);
    array_free(&a);
    return b;
}

Array
parse_hostinfo_replica(char *hostinfo)
{
    char *delim1 = ";";
    Array a = _delimit_by(hostinfo, delim1);
    char *delim = ",";
    Array b = _delimit_by(array_get(a, 1), delim);
    array_free(&a);
    return b;
}
