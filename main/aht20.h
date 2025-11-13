#pragma once

#include "i2c_bus.h"
#include "esp_err.h"

// Initialize AHT20 on the provided I2C bus
esp_err_t aht20_init(i2c_bus_handle_t i2c_bus);

// Trigger a measurement (if required). Many AHT20 chips start measurement on read; this is provided for completeness.
esp_err_t aht20_trigger_measurement(void);

// Read temperature in degrees Celsius
esp_err_t aht20_read_temperature(float *out_c);

// Read relative humidity in percent (0-100)
esp_err_t aht20_read_humidity(float *out_percent);
