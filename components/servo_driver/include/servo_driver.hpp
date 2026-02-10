/**
 * @file servo_driver.hpp
 * @brief PCA9685-based servo motor driver
 * 
 * Controls up to 16 servo motors using PCA9685 PWM driver via I2C
 */

#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <cstdint>

namespace Bobot {

/**
 * @brief PCA9685 servo motor driver class
 * 
 * Provides interface to control servo motors connected to PCA9685
 * 16-channel PWM driver. Supports standard servos with 50Hz PWM.
 */
class ServoDriver {
public:
    /**
     * @brief Construct a new Servo Driver object
     * 
     * @param bus_handle I2C master bus handle
     * @param i2c_address PCA9685 I2C address (default: 0x40)
     */
    ServoDriver(i2c_master_bus_handle_t bus_handle, uint8_t i2c_address = 0x40);

    /**
     * @brief Destroy the Servo Driver object
     */
    ~ServoDriver();

    /**
     * @brief Initialize the PCA9685 controller
     * 
     * Sets up PWM frequency to 50Hz for standard servos
     * 
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t init();

    /**
     * @brief Set servo angle
     * 
     * @param channel Servo channel (0-15)
     * @param angle Angle in degrees (0-180)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t setAngle(uint8_t channel, uint8_t angle);

    /**
     * @brief Set all servos to the same angle
     * 
     * @param angle Angle in degrees (0-180)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t setAllAngles(uint8_t angle);

    /**
     * @brief Set raw PWM pulse width in microseconds
     * 
     * @param channel Servo channel (0-15)
     * @param pulse_us Pulse width in microseconds (typically 500-2500)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t setPulseWidth(uint8_t channel, uint16_t pulse_us);

    /**
     * @brief Set servo angle smoothly with speed control
     * 
     * @param channel Servo channel (0-15)
     * @param target_angle Target angle in degrees (0-180)
     * @param speed_deg_per_sec Speed in degrees per second (60-600)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t setAngleSmooth(uint8_t channel, uint8_t target_angle, float speed_deg_per_sec);

    /**
     * @brief Update smooth servo movements (call periodically)
     * 
     * @param delta_ms Time since last update in milliseconds
     */
    void updateSmoothMovements(uint32_t delta_ms);

    /**
     * @brief Get current angle of a servo
     * 
     * @param channel Servo channel (0-15)
     * @return uint8_t Current angle (0-180)
     */
    uint8_t getCurrentAngle(uint8_t channel) const;

    /**
     * @brief Check if any servo is still moving
     * 
     * @return true if any servo is moving
     */
    bool isMoving() const;

private:
    i2c_master_dev_handle_t dev_handle;
    uint8_t address;

    // Servo state tracking
    struct ServoState {
        float current_angle;     // Current angle (float for smooth interpolation)
        float target_angle;      // Target angle
        float speed;             // Speed in degrees per second
        bool moving;             // Is servo currently moving
    };
    ServoState servo_states[16];

    /**
     * @brief Write byte to PCA9685 register
     */
    esp_err_t writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief Read byte from PCA9685 register
     */
    esp_err_t readRegister(uint8_t reg, uint8_t* value);

    /**
     * @brief Set PWM on/off times for a channel
     */
    esp_err_t setPWM(uint8_t channel, uint16_t on, uint16_t off);

    /**
     * @brief Set PWM frequency (typically 50Hz for servos)
     */
    esp_err_t setPWMFreq(float freq);
};

} // namespace Bobot
