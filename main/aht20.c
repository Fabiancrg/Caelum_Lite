#include "aht20.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdint.h>

static const char *TAG = "AHT20";
// Default I2C address for AHT20
#define AHT20_I2C_ADDR 0x38

static i2c_bus_device_handle_t s_dev = NULL;

// Measurement command for AHT20
static const uint8_t AHT20_CMD_MEASURE[3] = { 0xAC, 0x33, 0x00 };

esp_err_t aht20_init(i2c_bus_handle_t i2c_bus)
{
    if (!i2c_bus) return ESP_ERR_INVALID_ARG;

    // create device handle with default clock
    i2c_bus_device_handle_t dev = i2c_bus_device_create(i2c_bus, AHT20_I2C_ADDR, 0);
    if (dev == NULL) {
        ESP_LOGW(TAG, "aht20_init: device_create failed");
        return ESP_ERR_NOT_FOUND;
    }

    // Try to read a status byte to confirm presence
    uint8_t status = 0;
    esp_err_t ret = i2c_bus_read_bytes(dev, NULL_I2C_MEM_ADDR, 1, &status);
    if (ret == ESP_OK) {
        // status 0xFF would indicate no ACK/invalid data in some setups
        ESP_LOGI(TAG, "aht20_init: probe OK, status=0x%02x", status);
        s_dev = dev;
        return ESP_OK;
    }

    // cleanup
    i2c_bus_device_delete(&dev);
    ESP_LOGW(TAG, "aht20_init: probe failed");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t aht20_trigger_measurement(void)
{
    if (s_dev == NULL) return ESP_ERR_NOT_FOUND;

    esp_err_t ret = i2c_bus_write_bytes(s_dev, NULL_I2C_MEM_ADDR, sizeof(AHT20_CMD_MEASURE), AHT20_CMD_MEASURE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "aht20_trigger_measurement: write failed (%d)", ret);
        return ret;
    }

    // Wait typical conversion time (datasheet ~75ms). Use 80ms to be safe.
    vTaskDelay(pdMS_TO_TICKS(80));
    return ESP_OK;
}

static esp_err_t aht20_read_raw(uint8_t buf[6])
{
    if (s_dev == NULL) return ESP_ERR_NOT_FOUND;
    return i2c_bus_read_bytes(s_dev, NULL_I2C_MEM_ADDR, 6, buf);
}

esp_err_t aht20_read_temperature(float *out_c)
{
    if (!out_c) return ESP_ERR_INVALID_ARG;
    if (s_dev == NULL) return ESP_ERR_NOT_FOUND;

    // Trigger measurement then read
    esp_err_t ret = aht20_trigger_measurement();
    if (ret != ESP_OK) return ret;

    uint8_t raw[6];
    ret = aht20_read_raw(raw);
    if (ret != ESP_OK) return ret;

    // Parse raw: status, h[20], t[20]
    // humidity_raw = (raw[1]<<12) | (raw[2]<<4) | (raw[3] >> 4)
    // temp_raw = ((raw[3] & 0x0F) << 16) | (raw[4]<<8) | raw[5]
    uint32_t hum_raw = ((uint32_t)raw[1] << 12) | ((uint32_t)raw[2] << 4) | ((uint32_t)raw[3] >> 4);
    uint32_t temp_raw = (((uint32_t)raw[3] & 0x0F) << 16) | ((uint32_t)raw[4] << 8) | (uint32_t)raw[5];

    // convert to physical values per datasheet
    float humidity = ((float)hum_raw) * 100.0f / 1048576.0f; // 2^20 = 1048576
    float temperature = ((float)temp_raw) * 200.0f / 1048576.0f - 50.0f;

    (void)humidity; // humidity function will return a separate value
    *out_c = temperature;
    return ESP_OK;
}

esp_err_t aht20_read_humidity(float *out_percent)
{
    if (!out_percent) return ESP_ERR_INVALID_ARG;
    if (s_dev == NULL) return ESP_ERR_NOT_FOUND;

    esp_err_t ret = aht20_trigger_measurement();
    if (ret != ESP_OK) return ret;

    uint8_t raw[6];
    ret = aht20_read_raw(raw);
    if (ret != ESP_OK) return ret;

    uint32_t hum_raw = ((uint32_t)raw[1] << 12) | ((uint32_t)raw[2] << 4) | ((uint32_t)raw[3] >> 4);
    float humidity = ((float)hum_raw) * 100.0f / 1048576.0f;
    *out_percent = humidity;
    return ESP_OK;
}
