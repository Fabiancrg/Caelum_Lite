#pragma once

#include "i2c_bus.h"
#include "esp_err.h"

// Initialize BMP280 on provided I2C bus
esp_err_t bmp280_init(i2c_bus_handle_t i2c_bus);

// Trigger a measurement if required
esp_err_t bmp280_trigger_measurement(void);

// Read temperature in degrees Celsius
esp_err_t bmp280_read_temperature(float *out_c);

// Read pressure in hPa (triggers measurement internally)
esp_err_t bmp280_read_pressure(float *out_hpa);

// Read pressure in hPa without triggering (assumes measurement already done)
esp_err_t bmp280_read_pressure_no_trigger(float *out_hpa);
