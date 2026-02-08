/**
 * @file servo_driver.cpp
 * @brief Implementation of PCA9685 servo driver
 */

#include "servo_driver.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>

static const char* TAG = "ServoDriver";

// PCA9685 registers
#define PCA9685_MODE1 0x00
#define PCA9685_MODE2 0x01
#define PCA9685_PRESCALE 0xFE
#define PCA9685_LED0_ON_L 0x06
#define PCA9685_LED0_ON_H 0x07
#define PCA9685_LED0_OFF_L 0x08
#define PCA9685_LED0_OFF_H 0x09
#define PCA9685_ALL_LED_ON_L 0xFA
#define PCA9685_ALL_LED_OFF_L 0xFC

// Mode1 register bits
#define MODE1_RESTART 0x80
#define MODE1_SLEEP 0x10
#define MODE1_ALLCALL 0x01
#define MODE1_AI 0x20  // Auto-increment

// Oscillator frequency (internal)
#define PCA9685_CLOCK_FREQ 25000000.0f

// Servo pulse width range (microseconds)
#define SERVO_MIN_PULSE_US 500   // 0 degrees
#define SERVO_MAX_PULSE_US 2500  // 180 degrees

namespace Bobot {

ServoDriver::ServoDriver(i2c_master_bus_handle_t bus_handle, uint8_t i2c_address)
    : dev_handle(nullptr), address(i2c_address) {
    
    // Create I2C device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_address,
        .scl_speed_hz = 100000,  // 100kHz standard mode
        .scl_wait_us = 0,
        .flags = {},
    };
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
    }
}

ServoDriver::~ServoDriver() {
    if (dev_handle) {
        i2c_master_bus_rm_device(dev_handle);
    }
}

esp_err_t ServoDriver::writeRegister(uint8_t reg, uint8_t value) {
    uint8_t write_buf[2] = {reg, value};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
}

esp_err_t ServoDriver::readRegister(uint8_t reg, uint8_t* value) {
    return i2c_master_transmit_receive(dev_handle, &reg, 1, value, 1, pdMS_TO_TICKS(100));
}

esp_err_t ServoDriver::init() {
    ESP_LOGI(TAG, "Initializing PCA9685 at address 0x%02X", address);
    
    // Reset
    esp_err_t ret = writeRegister(PCA9685_MODE1, MODE1_RESTART);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MODE1 register: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set PWM frequency to 50Hz for servos
    ret = setPWMFreq(50.0f);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PWM frequency: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Enable auto-increment for easier multi-byte writes
    uint8_t mode1;
    ret = readRegister(PCA9685_MODE1, &mode1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MODE1: %s", esp_err_to_name(ret));
        return ret;
    }
    
    mode1 |= MODE1_AI;
    ret = writeRegister(PCA9685_MODE1, mode1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable auto-increment: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "PCA9685 initialized successfully");
    return ESP_OK;
}

esp_err_t ServoDriver::setPWMFreq(float freq) {
    // Calculate prescale value
    // prescale = round(osc_clock / (4096 * freq)) - 1
    float prescale_val = (PCA9685_CLOCK_FREQ / (4096.0f * freq)) - 1.0f;
    uint8_t prescale = (uint8_t)(prescale_val + 0.5f);
    
    ESP_LOGI(TAG, "Setting PWM frequency to %.2f Hz (prescale: %d)", freq, prescale);
    
    // Read current MODE1
    uint8_t old_mode;
    esp_err_t ret = readRegister(PCA9685_MODE1, &old_mode);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Put to sleep to change prescale
    uint8_t sleep_mode = (old_mode & 0x7F) | MODE1_SLEEP;
    ret = writeRegister(PCA9685_MODE1, sleep_mode);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Set prescale
    ret = writeRegister(PCA9685_PRESCALE, prescale);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Restore old mode
    ret = writeRegister(PCA9685_MODE1, old_mode);
    if (ret != ESP_OK) {
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(5));
    
    // Restart
    ret = writeRegister(PCA9685_MODE1, old_mode | MODE1_RESTART);
    return ret;
}

esp_err_t ServoDriver::setPWM(uint8_t channel, uint16_t on, uint16_t off) {
    if (channel > 15) {
        ESP_LOGE(TAG, "Invalid channel: %d (must be 0-15)", channel);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t reg_base = PCA9685_LED0_ON_L + (4 * channel);
    
    uint8_t write_buf[5];
    write_buf[0] = reg_base;
    write_buf[1] = on & 0xFF;
    write_buf[2] = (on >> 8) & 0xFF;
    write_buf[3] = off & 0xFF;
    write_buf[4] = (off >> 8) & 0xFF;
    
    esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C transmit failed for ch %d: %s", channel, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t ServoDriver::setPulseWidth(uint8_t channel, uint16_t pulse_us) {
    // At 50Hz, period is 20ms = 20000us
    // PCA9685 has 12-bit resolution (4096 steps)
    // pulse_ticks = (pulse_us / 20000) * 4096
    uint16_t pulse_ticks = (uint16_t)((pulse_us * 4096.0f) / 20000.0f);
    
    // Set ON time to 0, OFF time to pulse_ticks
    return setPWM(channel, 0, pulse_ticks);
}

esp_err_t ServoDriver::setAngle(uint8_t channel, uint8_t angle) {
    if (angle > 180) {
        ESP_LOGW(TAG, "Angle %d exceeds 180, clamping", angle);
        angle = 180;
    }
    
    // Map angle (0-180) to pulse width (500-2500us)
    uint16_t pulse_us = SERVO_MIN_PULSE_US + 
                        ((angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 180);
    
    esp_err_t ret = setPulseWidth(channel, pulse_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ch %d to %d°: %s", channel, angle, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t ServoDriver::setAllAngles(uint8_t angle) {
    esp_err_t ret = ESP_OK;
    for (uint8_t i = 0; i < 16; i++) {
        esp_err_t channel_ret = setAngle(i, angle);
        if (channel_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set angle for channel %d", i);
            ret = channel_ret;
        }
    }
    return ret;
}

} // namespace Bobot
