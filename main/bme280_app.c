#include "bme280_app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BME280_APP";
bme280_handle_t g_bme280 = NULL;

esp_err_t bme280_app_init(i2c_bus_handle_t i2c_bus)
{
    if (!i2c_bus) {
        ESP_LOGE(TAG, "i2c_bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    g_bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    if (!g_bme280) {
        ESP_LOGE(TAG, "Failed to create BME280 handle");
        return ESP_FAIL;
    }
    
    /* Configure BME280 for forced mode (sleep between measurements)
     * This minimizes power consumption - sensor sleeps until we trigger a measurement */
    esp_err_t err = bme280_set_sampling(g_bme280, 
                                       BME280_MODE_FORCED,           // Forced mode - sleep after each reading
                                       BME280_SAMPLING_X1,           // Temperature oversampling x1 (fast, low power)
                                       BME280_SAMPLING_X1,           // Pressure oversampling x1
                                       BME280_SAMPLING_X1,           // Humidity oversampling x1
                                       BME280_FILTER_OFF,            // No filtering needed for infrequent reads
                                       BME280_STANDBY_MS_0_5);       // Standby (not used in forced mode)
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BME280 forced mode config failed");
        return err;
    }
    
    /* Read calibration coefficients */
    err = bme280_read_coefficients(g_bme280);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BME280 calibration read failed");
        return err;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Brief settle time
    ESP_LOGI(TAG, "💤 BME280 initialized in FORCED mode (sleeps between measurements)");
    return ESP_OK;
}

esp_err_t bme280_app_sleep(void)
{
    if (!g_bme280) {
        return ESP_ERR_INVALID_STATE;
    }
    
    /* In forced mode, BME280 automatically returns to sleep after measurement.
     * This function is a no-op but kept for API consistency. */
    ESP_LOGD(TAG, "💤 BME280 in sleep mode (automatic in forced mode)");
    return ESP_OK;
}

esp_err_t bme280_app_wake_and_measure(void)
{
    if (!g_bme280) {
        ESP_LOGE(TAG, "BME280 handle is NULL");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Trigger a forced measurement - sensor will wake, measure, then return to sleep */
    esp_err_t err = bme280_take_forced_measurement(g_bme280);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to trigger forced measurement");
        return err;
    }
    
    /* Wait for measurement to complete (~10ms for 1x oversampling) */
    vTaskDelay(pdMS_TO_TICKS(15));
    
    ESP_LOGD(TAG, "⚡ BME280 forced measurement complete");
    return ESP_OK;
}

esp_err_t bme280_app_read_temperature(float *temperature)
{
    if (!g_bme280 || !temperature) return ESP_ERR_INVALID_ARG;
    return bme280_read_temperature(g_bme280, temperature);
}

esp_err_t bme280_app_read_humidity(float *humidity)
{
    if (!g_bme280 || !humidity) return ESP_ERR_INVALID_ARG;
    return bme280_read_humidity(g_bme280, humidity);
}

esp_err_t bme280_app_read_pressure(float *pressure)
{
    if (!g_bme280 || !pressure) return ESP_ERR_INVALID_ARG;
    return bme280_read_pressure(g_bme280, pressure);
}
