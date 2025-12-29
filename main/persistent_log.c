/*
 * Persistent Log System
 * Stores critical events in NVS for debugging intermittent issues
 * 
 * Design:
 * - Uses NVS instead of SPIFFS (simpler, built-in wear leveling)
 * - Stores up to 50 log entries (each ~128 bytes = 6.4KB total)
 * - Circular buffer: oldest entries overwritten when full
 * - Logs include timestamp, level, tag, and message
 */

#include "persistent_log.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "PLOG";
static const char *NVS_NAMESPACE = "plog";

#define MAX_LOG_ENTRIES 1000
#define MAX_MESSAGE_LEN 96  // Keep message concise to fit more entries

typedef struct {
    int64_t timestamp_us;   // Microseconds since boot
    char level;             // I/W/E/C
    char tag[16];          // Module name
    char message[MAX_MESSAGE_LEN];
} log_entry_t;

static nvs_handle_t nvs_plog_handle = 0;
static bool initialized = false;

esp_err_t persistent_log_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    // Open NVS namespace
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_plog_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized = true;
    ESP_LOGI(TAG, "Persistent log system initialized (max %d entries)", MAX_LOG_ENTRIES);
    return ESP_OK;
}

void persistent_log_add(char level, const char *tag, const char *message)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Not initialized, skipping log");
        return;
    }

    // Get current entry count
    uint32_t count = 0;
    nvs_get_u32(nvs_plog_handle, "count", &count);

    // Circular buffer index
    uint32_t idx = count % MAX_LOG_ENTRIES;

    // Create log entry
    log_entry_t entry = {0};
    entry.timestamp_us = esp_timer_get_time();
    entry.level = level;
    snprintf(entry.tag, sizeof(entry.tag), "%s", tag);
    snprintf(entry.message, sizeof(entry.message), "%s", message);

    // Store entry
    char key[16];
    snprintf(key, sizeof(key), "log_%lu", (unsigned long)idx);
    
    esp_err_t ret = nvs_set_blob(nvs_plog_handle, key, &entry, sizeof(log_entry_t));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write log entry: %s", esp_err_to_name(ret));
        return;
    }

    // Update count
    count++;
    nvs_set_u32(nvs_plog_handle, "count", count);
    nvs_commit(nvs_plog_handle);

    // Also log to console for immediate visibility
    ESP_LOGI(TAG, "[PERSISTENT] %c/%s: %s", level, tag, message);
}

void persistent_log_dump_and_clear(void)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }

    // Get count
    uint32_t count = 0;
    nvs_get_u32(nvs_plog_handle, "count", &count);

    if (count == 0) {
        ESP_LOGI(TAG, "📋 No persistent logs stored");
        return;
    }

    ESP_LOGI(TAG, "📋 ========== PERSISTENT LOGS FROM PREVIOUS SESSION ==========");
    ESP_LOGI(TAG, "Total entries: %lu (showing last %d)", 
             (unsigned long)count, count > MAX_LOG_ENTRIES ? MAX_LOG_ENTRIES : (int)count);

    // Determine which entries to read
    uint32_t start_idx = count > MAX_LOG_ENTRIES ? (count - MAX_LOG_ENTRIES) : 0;
    uint32_t entries_to_read = count > MAX_LOG_ENTRIES ? MAX_LOG_ENTRIES : count;

    for (uint32_t i = 0; i < entries_to_read; i++) {
        uint32_t actual_idx = (start_idx + i) % MAX_LOG_ENTRIES;
        
        char key[16];
        snprintf(key, sizeof(key), "log_%lu", (unsigned long)actual_idx);
        
        log_entry_t entry = {0};
        size_t required_size = sizeof(log_entry_t);
        
        esp_err_t ret = nvs_get_blob(nvs_plog_handle, key, &entry, &required_size);
        if (ret == ESP_OK) {
            // Convert timestamp to seconds.milliseconds
            int64_t seconds = entry.timestamp_us / 1000000;
            int64_t millis = (entry.timestamp_us % 1000000) / 1000;
            
            ESP_LOGI(TAG, "[%lld.%03lld] %c/%s: %s", 
                     (long long)seconds, (long long)millis,
                     entry.level, entry.tag, entry.message);
        }
    }

    ESP_LOGI(TAG, "📋 ============================================================");
}

uint32_t persistent_log_get_count(void)
{
    if (!initialized) {
        return 0;
    }

    uint32_t count = 0;
    nvs_get_u32(nvs_plog_handle, "count", &count);
    return count;
}

void persistent_log_clear(void)
{
    if (!initialized) {
        return;
    }

    // Just reset the count - old entries will be overwritten
    nvs_set_u32(nvs_plog_handle, "count", 0);
    nvs_commit(nvs_plog_handle);
    ESP_LOGI(TAG, "Persistent logs cleared");
}
