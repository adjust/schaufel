#include <file.h>
#include <errno.h>

/* TODO:
 * lots of error handling
 */

typedef struct Meta {
    FILE *fp;
} *Meta;

Meta
file_meta_init(char *fname, char *options)
{
    Meta m = calloc(1, sizeof(*m));
    if (m == NULL)
    {
        logger_log("%s %d: allocate failed", __FILE__, __LINE__);
        abort();
    }
    m->fp = fopen(fname, options);
    if (m->fp == NULL)
    {
        logger_log("%s %d: %s", __FILE__, __LINE__, strerror(errno));
        abort();
    }
    return m;
}

void
file_meta_free(Meta *m)
{
    if ( fclose((*m)->fp) != 0)
        logger_log("%s %d: %s", __FILE__, __LINE__, strerror(errno));
    free(*m);
    *m = NULL;
}

Producer
file_producer_init(char *fname)
{
    Producer file = calloc(1, sizeof(*file));
    if (file == NULL)
        logger_log("%s %d: allocate failed", __FILE__, __LINE__);

    file->meta          = file_meta_init(fname, "a");
    file->producer_free = file_producer_free;
    file->produce       = file_producer_produce;

    return file;
}

void
file_producer_produce(Producer p, Message msg)
{
    char *line = message_get_data(msg);
    char *newline = "\n";
    fwrite(line, strlen(line), sizeof(*line),((Meta) p->meta)->fp);
    fwrite(newline, 1, 1,((Meta) p->meta)->fp);
}

void
file_producer_free(Producer *p)
{
    Meta m = (Meta) ((*p)->meta);
    file_meta_free(&m);
    free(*p);
    *p = NULL;
}

Consumer
file_consumer_init(char *fname)
{
    Consumer file = calloc(1, sizeof(*file));
    if (file == NULL)
        logger_log("%s %d: allocate failed", __FILE__, __LINE__);

    file->meta          = file_meta_init(fname, "r");
    file->consumer_free = file_consumer_free;
    file->consume       = file_consumer_consume;

    return file;
}

void
file_consumer_consume(Consumer c, Message msg)
{
    char *line = NULL;
    size_t read = 0;
    if (getline(&line, &read, ((Meta) c->meta)->fp) == -1)
        logger_log("%s %d: %s", __FILE__, __LINE__, strerror(errno));
    message_set_data(msg, line);
}

void
file_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    file_meta_free(&m);
    free(*c);
    *c = NULL;
}
