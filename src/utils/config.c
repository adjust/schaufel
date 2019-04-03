#include <utils/config.h>

int get_thread_count(config_t* config, int type)
{
    config_setting_t* threads = NULL;
    int list = 0, j = 0, result = 0;

    char *typestr;
    char *buf = malloc(1024);

    switch(type)
    {
        case(SCHAUFEL_TYPE_CONSUMER):
            typestr = "consumers";
            break;
        case(SCHAUFEL_TYPE_PRODUCER):
            typestr = "producers";
            break;
        default:
            abort();
    }
    threads = config_lookup(config, typestr);

    list = config_setting_length(threads);
    for(int i = 0; i < list; i++) {
        snprintf(buf, 1023, "%s.[%d].threads", typestr, i);
        config_lookup_int(config,buf,&j);
        result += j;
    }

    free(buf);
    return result;
}

static config_setting_t *
_add_node(config_setting_t* config, char* name, int type)
{
    config_setting_t *child = NULL;
    // name is ignored if config is a list
    if(name)
        config_setting_remove(config, name);
    child = config_setting_add(config, name, type);
    if(!child)
        abort();
    return child;
}

static config_setting_t *
_add_member(config_setting_t* config, char* name, int type)
{
    config_setting_t *child;
    child = config_setting_add(config, name, type);
    if(!child)
        abort();
    return(child);
}

char *
module_to_string(int module)
{
    char *result = NULL;
    switch(module) {
        case 'd':
            result = "dummy";
            break;
        case 'r':
            result = "redis";
            break;
        case 'k':
            result = "kafka";
            break;
        case 'p':
            result = "postgres";
            break;
        case 'f':
            result = "file";
            break;
        default:
            abort();
    }
    return result;
}

static bool _thread_validate(config_t* config, int type)
{
    char* buf = calloc(1,1024);
    config_setting_t *setting = NULL, *child = NULL;

    unsigned int i, list;
    int conf_i = 0;

    const char *conf_str = NULL;
    char *typestr;

    Validator v;
    bool ret = true;

    switch(type)
    {
        case(SCHAUFEL_TYPE_CONSUMER):
            typestr = "consumers";
            break;
        case(SCHAUFEL_TYPE_PRODUCER):
            typestr = "producers";
            break;
        default:
            abort();
    }

    setting = config_lookup(config, typestr);
    if (!setting) {
        fprintf(stderr, "Need a %s list\n", typestr);
        ret = false;
        goto error;
    }
    if(config_setting_is_list(setting) != CONFIG_TRUE) {
        fprintf(stderr, "%s needs to be a list\n", typestr);
        ret = false;
        goto error;
    }

    list = config_setting_length(setting);
    for(i = 0; i < list; i++) {
        snprintf(buf, 1023, "%s.[%d].threads", typestr, i);
        if(config_lookup_int(config,buf,&conf_i)!= CONFIG_TRUE
            || conf_i <= 0 ) {
            fprintf(stderr, "%s: [%d] need threads\n", typestr, i);
            ret = false;
        }
        snprintf(buf, 1023, "%s.[%d].type", typestr, i);
        if(config_lookup_string(config,buf,&conf_str)!= CONFIG_TRUE) {
            fprintf(stderr, "%s: [%d] needs a type\n", typestr, i);
            ret = false;
        }

        child = config_setting_get_elem(setting, i);
        config_setting_lookup_string(child, "type", &conf_str);

        v = validator_init((char *) conf_str);

        if(type == SCHAUFEL_TYPE_CONSUMER) {
            if(!v->validate_consumer(child)) {
                ret = false;
                goto error;
            }
        }
        if(type == SCHAUFEL_TYPE_PRODUCER) {
            if(!v->validate_producer(child)) {
                ret = false;
                goto error;
            }
        }
        free(v);
    }

    error:
    free(buf);
    return(ret);
}

bool config_validate(config_t* config)
{
    bool res = true;
    config_setting_t *setting;

    // check logger
    setting = config_lookup(config, "logger");
    if (!setting)
    {
        fprintf(stderr, "Need a logger defined\n");
        res = false;
    }
    if(!logger_validate(setting)) {
        res = false;
    }

    //check consumers
    if(!_thread_validate(config, SCHAUFEL_TYPE_CONSUMER))
        res = false;
    //check producers
    if(!_thread_validate(config, SCHAUFEL_TYPE_PRODUCER))
        res = false;

    return res;
}

void
config_merge(config_t* config, Options o)
{

    if (o.config) {
        read_config(config, o.config);
    }

    /* Commandline options take precedence over
     * config file parameters. */
    config_setting_t *croot = NULL, *parent = NULL, *setting = NULL;
    croot = config_root_setting(config);

    setting = config_lookup(config, "logger");
    if (o.logger) {
        // cmdline logger only knows of files
        parent = _add_node(croot, "logger", CONFIG_TYPE_GROUP);
        setting = _add_member(parent, "file", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, o.logger);
        setting = _add_member(parent, "type", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, "file");
    } // TODO : default to stderr

    //consumers
    if (o.input) {
        parent = _add_node(croot, "consumers", CONFIG_TYPE_LIST);
        parent = _add_node(parent, NULL, CONFIG_TYPE_GROUP);
        setting = _add_member(parent, "type", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, module_to_string(o.input));

        // threads are initialized to 0, therefore they are safe to add
        setting = _add_member(parent, "threads", CONFIG_TYPE_INT);
        config_setting_set_int(setting, o.consumer_threads);
        if (o.in_broker) {
            setting = _add_member(parent, "broker", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.in_broker);
        }
        if (o.in_host) {
            setting = _add_member(parent, "host", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.in_host);
        }
        if (o.in_groupid) {
            setting = _add_member(parent, "groupid", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.in_groupid);
        }
        if (o.in_topic) {
            setting = _add_member(parent, "topic", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.in_topic);
        }
        if (o.in_file) {
            setting = _add_member(parent, "file", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.in_file);
        }
        if (o.in_pipeline) {
            setting = _add_member(parent, "pipeline", CONFIG_TYPE_INT);
            config_setting_set_int(setting, o.in_pipeline);
        }
    }

    //producers
    if (o.output) {
        parent = _add_node(croot, "producers", CONFIG_TYPE_LIST);
        parent = _add_node(parent, NULL, CONFIG_TYPE_GROUP);
        setting = _add_member(parent, "type", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, module_to_string(o.output));

        // threads are initialized to 0, therefore they are safe to add
        setting = _add_member(parent, "threads", CONFIG_TYPE_INT);
        config_setting_set_int(setting, o.producer_threads);
        if (o.out_broker) {
            setting = _add_member(parent, "broker", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.out_broker);
        }
        if (o.out_host) {
            setting = _add_member(parent, "host", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.out_host);
        }
        if (o.out_groupid) {
            setting = _add_member(parent, "groupid", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.out_groupid);
        }
        if (o.out_topic) {
            setting = _add_member(parent, "topic", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.out_topic);
        }
        if (o.out_file) {
            setting = _add_member(parent, "file", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, o.out_file);
        }
        if (o.out_pipeline) {
            setting = _add_member(parent, "pipeline", CONFIG_TYPE_INT);
            config_setting_set_int(setting, o.out_pipeline);
        }
    }
}

void read_config(config_t* config,char* cfile)
{
    if(config_read_file(config, cfile) != CONFIG_TRUE) {
        fprintf(stderr, "%s %d: failed to read config: %s\n",
            __FILE__, __LINE__,
            config_error_text(config));
        if(config_error_line(config)){
            fprintf(stderr,
            "%s: line %d\n", cfile, config_error_line(config));
        }
        exit(1);
    }
}
