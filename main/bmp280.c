#include "bmp280.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdint.h>

static const char *TAG = "BMP280";

// Possible I2C addresses
#define BMP280_ADDR_0 0x76
#define BMP280_ADDR_1 0x77

// Registers
#define BMP280_REG_ID         0xD0
#define BMP280_REG_RESET      0xE0
#define BMP280_REG_CALIB00    0x88
#define BMP280_REG_CTRL_MEAS  0xF4
#define BMP280_REG_CONFIG     0xF5
#define BMP280_REG_DATA       0xF7

static i2c_bus_device_handle_t s_dev = NULL;

/* Calibration values */
static uint16_t dig_T1;
static int16_t  dig_T2;
static int16_t  dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2;
static int16_t  dig_P3;
static int16_t  dig_P4;
static int16_t  dig_P5;
static int16_t  dig_P6;
static int16_t  dig_P7;
static int16_t  dig_P8;
static int16_t  dig_P9;

static int32_t t_fine = 0;

static esp_err_t bmp280_read_calibration(i2c_bus_device_handle_t dev)
{
    uint8_t calib[24];
    esp_err_t ret = i2c_bus_read_bytes(dev, BMP280_REG_CALIB00, sizeof(calib), calib);
    if (ret != ESP_OK) return ret;

    dig_T1 = (uint16_t)((calib[1] << 8) | calib[0]);
    dig_T2 = (int16_t)((calib[3] << 8) | calib[2]);
    dig_T3 = (int16_t)((calib[5] << 8) | calib[4]);
    dig_P1 = (uint16_t)((calib[7] << 8) | calib[6]);
    dig_P2 = (int16_t)((calib[9] << 8) | calib[8]);
    dig_P3 = (int16_t)((calib[11] << 8) | calib[10]);
    dig_P4 = (int16_t)((calib[13] << 8) | calib[12]);
    dig_P5 = (int16_t)((calib[15] << 8) | calib[14]);
    dig_P6 = (int16_t)((calib[17] << 8) | calib[16]);
    dig_P7 = (int16_t)((calib[19] << 8) | calib[18]);
    dig_P8 = (int16_t)((calib[21] << 8) | calib[20]);
    dig_P9 = (int16_t)((calib[23] << 8) | calib[22]);
    return ESP_OK;
}

esp_err_t bmp280_init(i2c_bus_handle_t i2c_bus)
{
    if (!i2c_bus) return ESP_ERR_INVALID_ARG;

    i2c_bus_device_handle_t dev = i2c_bus_device_create(i2c_bus, BMP280_ADDR_0, 0);
    uint8_t id = 0;

    if (dev) {
        if (i2c_bus_read_bytes(dev, BMP280_REG_ID, 1, &id) == ESP_OK && id == 0x58) {
            ESP_LOGI(TAG, "bmp280_init: found BMP280 at 0x%02x", BMP280_ADDR_0);
            if (bmp280_read_calibration(dev) == ESP_OK) {
                s_dev = dev;
                return ESP_OK;
            }
        }
        i2c_bus_device_delete(&dev);
    }

    // try alternate address
    dev = i2c_bus_device_create(i2c_bus, BMP280_ADDR_1, 0);
    if (dev) {
        if (i2c_bus_read_bytes(dev, BMP280_REG_ID, 1, &id) == ESP_OK && id == 0x58) {
            ESP_LOGI(TAG, "bmp280_init: found BMP280 at 0x%02x", BMP280_ADDR_1);
            if (bmp280_read_calibration(dev) == ESP_OK) {
                s_dev = dev;
                return ESP_OK;
            }
        }
        i2c_bus_device_delete(&dev);
    }

    ESP_LOGW(TAG, "bmp280_init: probe failed");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t bmp280_trigger_measurement(void)
{
    if (s_dev == NULL) return ESP_ERR_NOT_FOUND;
    // set oversampling x1 for temp and pressure, and forced mode (0x25)
    uint8_t ctrl = (1 << 5) | (1 << 2) | 1; // osrs_t=1, osrs_p=1, mode=1 (forced)
    return i2c_bus_write_bytes(s_dev, BMP280_REG_CTRL_MEAS, 1, &ctrl);
}

static esp_err_t bmp280_read_raw(int32_t *adc_T, int32_t *adc_P)
{
    if (!adc_T || !adc_P) return ESP_ERR_INVALID_ARG;
    if (s_dev == NULL) return ESP_ERR_NOT_FOUND;

    // trigger forced measurement
    esp_err_t ret = bmp280_trigger_measurement();
    if (ret != ESP_OK) return ret;

    // typical max conversion time ~10ms for osrs=1; wait 15ms
    vTaskDelay(pdMS_TO_TICKS(15));

    uint8_t data[6];
    ret = i2c_bus_read_bytes(s_dev, BMP280_REG_DATA, 6, data);
    if (ret != ESP_OK) return ret;

    *adc_P = (int32_t)((((uint32_t)data[0]) << 12) | (((uint32_t)data[1]) << 4) | ((uint32_t)data[2] >> 4));
    *adc_T = (int32_t)((((uint32_t)data[3]) << 12) | (((uint32_t)data[4]) << 4) | ((uint32_t)data[5] >> 4));
    return ESP_OK;
}

esp_err_t bmp280_read_temperature(float *out_c)
{
    if (!out_c) return ESP_ERR_INVALID_ARG;
    int32_t adc_T = 0, adc_P = 0;
    esp_err_t ret = bmp280_read_raw(&adc_T, &adc_P);
    if (ret != ESP_OK) return ret;

    // Temperature compensation (per datasheet)
    float var1 = (((float)adc_T) / 16384.0f - ((float)dig_T1) / 1024.0f) * ((float)dig_T2);
    float var2 = ((((float)adc_T) / 131072.0f - ((float)dig_T1) / 8192.0f) * (((float)adc_T) / 131072.0f - ((float)dig_T1) / 8192.0f)) * ((float)dig_T3);
    float T = var1 + var2;
    t_fine = (int32_t)T;
    *out_c = T / 5120.0f;
    return ESP_OK;
}

esp_err_t bmp280_read_pressure(float *out_hpa)
{
    if (!out_hpa) return ESP_ERR_INVALID_ARG;
    int32_t adc_T = 0, adc_P = 0;
    esp_err_t ret = bmp280_read_raw(&adc_T, &adc_P);
    if (ret != ESP_OK) return ret;

    // compute t_fine from adc_T
    float var1 = (((float)adc_T) / 16384.0f - ((float)dig_T1) / 1024.0f) * ((float)dig_T2);
    float var2 = ((((float)adc_T) / 131072.0f - ((float)dig_T1) / 8192.0f) * (((float)adc_T) / 131072.0f - ((float)dig_T1) / 8192.0f)) * ((float)dig_T3);
    float tf = var1 + var2;
    int32_t t_f = (int32_t)tf;

    // Pressure compensation (per datasheet)
    float p_var1 = ((float)t_f / 2.0f) - 64000.0f;
    float p_var2 = p_var1 * p_var1 * ((float)dig_P6) / 32768.0f;
    p_var2 = p_var2 + p_var1 * ((float)dig_P5) * 2.0f;
    p_var2 = (p_var2 / 4.0f) + (((float)dig_P4) * 65536.0f);
    p_var1 = (((float)dig_P3) * p_var1 * p_var1 / 524288.0f + ((float)dig_P2) * p_var1) / 524288.0f;
    p_var1 = (1.0f + p_var1 / 32768.0f) * ((float)dig_P1);
    if (p_var1 == 0.0f) return ESP_ERR_INVALID_STATE; // avoid division by zero

    float p = 1048576.0f - (float)adc_P;
    p = (p - (p_var2 / 4096.0f)) * 6250.0f / p_var1;
    p_var1 = ((float)dig_P9) * p * p / 2147483648.0f;
    p_var2 = p * ((float)dig_P8) / 32768.0f;
    p = p + (p_var1 + p_var2 + ((float)dig_P7)) / 16.0f;

    *out_hpa = p / 100.0f; // convert Pa to hPa
    return ESP_OK;
}
