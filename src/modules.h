#ifndef MODULES_H
#define MODULES_H

#include <libconfig.h>
#include <stdbool.h>

#include "consumer.h"
#include "producer.h"


typedef struct ModuleHandler
{
    /* consumer routines */
    Consumer  (*consumer_init) (config_setting_t *config);
    int       (*consume) (Consumer c, Message msg);
    void      (*consumer_free) (Consumer *c);
    
    /* producer routines */
    Producer  (*producer_init) (config_setting_t *config);
    void      (*produce) (Producer p, Message msg);
    void      (*producer_free) (Producer *p);

    /* validator routines */
    bool      (*validate_consumer) (config_setting_t* config);
    bool      (*validate_producer) (config_setting_t* config);
} ModuleHandler;

extern void register_module(const char *name, ModuleHandler *handler);
ModuleHandler *lookup_module(const char *name);
bool load_module(const char *sopath);

/* TODO: temporary */
void register_builtin_modules(void);

#endif
