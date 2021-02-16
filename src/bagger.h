#ifndef _SCHAUFEL_BAGGER_H_
#define _SCHAUFEL_BAGGER_H_

#include <libconfig.h>

#include "producer.h"
#include "consumer.h"
#include "validator.h"
#include "queue.h"


Producer bagger_producer_init(config_setting_t *config);
Consumer bagger_consumer_init(config_setting_t *config);
Validator bagger_validator_init();

#endif
