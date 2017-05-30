#include <utils/helper.h>

int
_file_consumer_validate(Options o)
{
    if (o.in_file == NULL)
    {
        logger_log("%s %d: Missing in_file parameter", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int
_redis_consumer_validate(Options o)
{
    if (o.in_host == NULL)
    {
        logger_log("%s %d: Missing in_host parameter", __FILE__, __LINE__);
        return -1;
    }
    if (o.in_topic == NULL)
    {
        logger_log("%s %d: Missing in_topic parameter", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int
_kafka_consumer_validate(Options o)
{
    if (o.in_broker == NULL)
    {
        logger_log("%s %d: Missing in_broker parameter", __FILE__, __LINE__);
        return -1;
    }
    if (o.in_topic == NULL)
    {
        logger_log("%s %d: Missing in_topic parameter", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int
_file_producer_validate(Options o)
{
    if (o.out_file == NULL)
    {
        logger_log("%s %d: Missing out_file parameter", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int
_redis_producer_validate(Options o)
{
    if (o.out_host == NULL)
    {
        logger_log("%s %d: Missing out_host parameter", __FILE__, __LINE__);
        return -1;
    }
    if (o.out_topic == NULL)
    {
        logger_log("%s %d: Missing out_topic parameter", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int
_kafka_producer_validate(Options o)
{
    if (o.out_broker == NULL)
    {
        logger_log("%s %d: Missing out_broker parameter", __FILE__, __LINE__);
        return -1;
    }
    if (o.out_topic == NULL)
    {
        logger_log("%s %d: Missing out_topic parameter", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int
_postgres_producer_validate(Options o)
{
    if (o.out_host == NULL)
    {
        logger_log("%s %d: Missing out_host parameter", __FILE__, __LINE__);
        return -1;
    }
    if (o.out_topic == NULL)
    {
        logger_log("%s %d: Missing out_topic parameter", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int
options_validate(Options o)
{
    int ok = 1;

    if (o.logger == NULL)
    {
        fprintf(stderr, "need a logfile\n");
        return -1;
    }
    switch (o.input)
    {
        case 'd':
            break;
        case 'f':
            ok += _file_consumer_validate(o);
            break;
        case 'r':
            ok += _redis_consumer_validate(o);
            break;
        case 'k':
            ok += _kafka_consumer_validate(o);
            break;
    }

    switch (o.output)
    {
        case 'd':
            break;
         case 'f':
            ok += _file_producer_validate(o);
            break;
        case 'r':
            ok += _redis_producer_validate(o);
            break;
        case 'k':
            ok += _kafka_producer_validate(o);
            break;
        case 'p':
            ok += _postgres_producer_validate(o);
    }

    return ok;
}

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
