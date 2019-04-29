#ifndef _SCHAUFEL_LOGGER_H_
#define _SCHAUFEL_LOGGER_H_

#include <libconfig.h>
#include <stdbool.h>
#include <stdio.h>


#define LOG_BUFFER_SIZE 4096
#define FORMAT_PRINTF(x,y) __attribute__((format (printf,(x),(y))))

bool get_logger_state();
bool logger_validate(config_setting_t *config);
void logger_init(config_setting_t *config);
void logger_free();
void logger_log(const char *fmt, ...) FORMAT_PRINTF(1,2);

#endif
