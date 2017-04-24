#include <schaufel.h>

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
    if (o.in_port == 0)
    {
        logger_log("%s %d: Missing in_port parameter", __FILE__, __LINE__);
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
    if (o.out_port == 0)
    {
        logger_log("%s %d: Missing out_port parameter", __FILE__, __LINE__);
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
    if (o.in_topic == NULL)
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
    if (o.out_port == 0)
    {
        logger_log("%s %d: Missing out_port parameter", __FILE__, __LINE__);
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
    char *save;
    char *port_str;
    *hostname = strdup(strtok_r(conninfo, delim, &save));
    if (*hostname == NULL)
        return -1;
    port_str = strtok_r(NULL, delim, &save);
    if (port_str == NULL)
        return 1;
    *port = atoi(port_str);
    if (*port == 0)
        return -1;
    return 0;
}
