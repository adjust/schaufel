#include "schaufel.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "utils/logger.h"
#include "utils/scalloc.h"


/* TODO:
 * lots of error handling
 */

typedef struct Meta {
    FILE *fp;
} *Meta;

Meta
file_meta_init(const char *fname, char *options)
{
    Meta m = SCALLOC(1, sizeof(*m));

    m->fp = fopen(fname, options);
    if (m->fp == NULL)
    {
        logger_log("%s %d: %s %s", __FILE__, __LINE__, fname, strerror(errno));
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
file_producer_init(config_setting_t *config)
{
    Producer file = SCALLOC(1, sizeof(*file));
    const char *fname = NULL;
    config_setting_lookup_string(config, "file", &fname);

    file->meta          = file_meta_init(fname, "a");
    file->producer_free = file_producer_free;
    file->produce       = file_producer_produce;

    return file;
}

void
file_producer_produce(Producer p, Message msg)
{
    char *line = message_get_data(msg);
    size_t len = message_get_len(msg);

    fwrite(line, len, sizeof(*line),((Meta) p->meta)->fp);
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
file_consumer_init(config_setting_t *config)
{
    Consumer file = SCALLOC(1, sizeof(*file));
    const char *fname = NULL;
    config_setting_lookup_string(config, "file", &fname);

    file->meta          = file_meta_init(fname, "r");
    file->consumer_free = file_consumer_free;
    file->consume       = file_consumer_consume;
    return file;
}

int
file_consumer_consume(Consumer c, Message msg)
{
    char   *line = NULL;
    size_t  bufsize = 0;
    ssize_t read;
    int8_t err = errno;
    errno = 0;
    if ((read = getline(&line, &bufsize, ((Meta) c->meta)->fp)) == -1)
    {
        /* if a line buffer was allocated, it needs freeing.
         * If it was not allocated, it is NULL and therefore save
         * to free */
        free(line);

        /* We have reached EOF */
        if(!(errno == EINVAL || errno == ENOMEM))
            logger_log("%s %d: reached EOF", __FILE__, __LINE__);
        else
            logger_log("%s %d: %s", __FILE__, __LINE__, strerror(errno));

        return -1;
    }
    errno = err;
    message_set_data(msg, line);
    message_set_len(msg, (size_t) read);
    return 0;
}

void
file_consumer_free(Consumer *c)
{
    Meta m = (Meta) ((*c)->meta);
    file_meta_free(&m);
    free(*c);
    *c = NULL;
}

bool
file_validate(config_setting_t* config)
{
    int t = 0;
    config_setting_lookup_int(config, "threads", &t);

    if(t > 1) {
        fprintf(stderr, "file consumer/producer is not thread safe!\n");
        return false;
    }

    return true;
}

Validator
file_validator_init()
{
    Validator v = SCALLOC(1,sizeof(*v));

    v->validate_producer = file_validate;
    v->validate_consumer = file_validate;
    return v;
}
