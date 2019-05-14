#ifndef MODULES_H
#define MODULES_H

#include <libconfig.h>

#include "consumer.h"
#include "producer.h"
#include "validator.h"


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

    /* validator routine */
    Validator (*validator_init) (void);
} ModuleHandler;

extern void register_module(const char *name, ModuleHandler *handler);
ModuleHandler *lookup_module(const char *name);

/* TODO: temporary */
void register_builtin_modules(void);

#endif
