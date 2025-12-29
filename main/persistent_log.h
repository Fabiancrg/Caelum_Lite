/*
 * Persistent Log System
 * Logs critical events to flash for post-mortem analysis
 */

#ifndef PERSISTENT_LOG_H
#define PERSISTENT_LOG_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize persistent log system
 * @return ESP_OK on success
 */
esp_err_t persistent_log_init(void);

/**
 * @brief Add a log entry to persistent storage
 * @param level Log level (I=Info, W=Warning, E=Error, C=Critical)
 * @param tag Tag/module name
 * @param message Log message
 */
void persistent_log_add(char level, const char *tag, const char *message);

/**
 * @brief Print all stored logs to console and clear them
 */
void persistent_log_dump_and_clear(void);

/**
 * @brief Get number of stored log entries
 * @return Number of log entries
 */
uint32_t persistent_log_get_count(void);

/**
 * @brief Clear all stored logs
 */
void persistent_log_clear(void);

#ifdef __cplusplus
}
#endif

#endif // PERSISTENT_LOG_H
