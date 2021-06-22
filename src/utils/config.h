#ifndef _SCHAUFEL_UTILS_CONFIG_H_
#define _SCHAUFEL_UTILS_CONFIG_H_

#define SCHAUFEL_TYPE_CONSUMER 1
#define SCHAUFEL_TYPE_PRODUCER 2

#include <libconfig.h>
#include <stdbool.h>

#include "utils/options.h"

typedef void (*group_func)(const char *key, const char *value, void *arg);

void logger_parse(char *, config_setting_t *);

void read_config(config_t *config, char *cfile);

void config_merge(config_t *config, Options o);

int get_thread_count(config_t *config, int type);

bool config_validate(config_t *config);

char* module_to_string(int module);

void config_group_apply(const config_setting_t *options, group_func func, void *arg);

config_setting_t *config_create_path(config_setting_t *parent,
                                     const char *path,
                                     int type);

void config_set_default_string(config_setting_t *parent, const char *path,
   const char *value);

/* Macros to abstract config parsing
 * Check for existance of a config item */
bool conf_lookup_is_string(config_setting_t *conf, const char *path, const char **res,
    const char *file, size_t line, const char *err);
bool conf_lookup_is_int(config_setting_t *conf, const char *path, int *res,
    const char *file, size_t line, const char *err);
config_setting_t *conf_get_member(config_setting_t *conf, const char *name,
    const char *file, size_t line, const char *err);
bool conf_is_list(config_setting_t *conf,
    const char *file, size_t line, const char *err);
#define CONF_L_IS_STRING(conf, path, res, err) \
    conf_lookup_is_string(conf, path, res, __FILE__, __LINE__, err)
#define CONF_L_IS_INT(conf, path, res, err) \
    conf_lookup_is_int(conf, path, res, __FILE__, __LINE__, err)
#define CONF_GET_MEM(conf, name, err) \
    conf_get_member(conf, name, __FILE__, __LINE__, err)
#define CONF_IS_LIST(conf, err) \
    conf_is_list(conf, __FILE__, __LINE__, err)
#endif
