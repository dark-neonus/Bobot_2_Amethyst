/**
 * @file buzzer.hpp
 * @brief PWM-controlled buzzer driver
 * 
 * Features:
 * - Controls buzzer intensity using PWM
 * - Configurable duty cycle (0-100%)
 * - Simple on/off control
 */

#pragma once

#include <esp_err.h>
#include <hal/ledc_types.h>
#include <hal/gpio_types.h>
#include <cstdint>

namespace Bobot {

/**
 * @brief Buzzer driver class with PWM control
 */
class Buzzer {
public:
    /**
     * @brief Configuration for buzzer
     */
    struct Config {
        gpio_num_t pin;           ///< GPIO pin for buzzer (default: GPIO23)
        uint32_t frequency;       ///< PWM frequency in Hz (default: 2000)
        ledc_timer_t timer;       ///< LEDC timer to use (default: LEDC_TIMER_0)
        ledc_channel_t channel;   ///< LEDC channel to use (default: LEDC_CHANNEL_0)
        ledc_mode_t mode;         ///< LEDC mode (default: LEDC_LOW_SPEED_MODE)
    };

    /**
     * @brief Constructor with default configuration
     */
    Buzzer();

    /**
     * @brief Destructor
     */
    ~Buzzer();

    /**
     * @brief Initialize buzzer with configuration
     * @param config Configuration structure
     * @return ESP_OK on success
     */
    esp_err_t init(const Config& config);

    /**
     * @brief Turn buzzer on with current duty cycle
     */
    void on();

    /**
     * @brief Turn buzzer off
     */
    void off();

    /**
     * @brief Set buzzer duty cycle (intensity)
     * @param duty_percent Duty cycle percentage (0-100)
     */
    void setDutyCycle(uint8_t duty_percent);

    /**
     * @brief Get current duty cycle
     * @return Current duty cycle percentage (0-100)
     */
    uint8_t getDutyCycle() const;

    /**
     * @brief Check if buzzer is currently on
     * @return true if on, false if off
     */
    bool isOn() const;

    /**
     * @brief Toggle buzzer on/off state
     */
    void toggle();

private:
    Config config_;
    uint8_t duty_percent_;
    bool is_on_;
    bool initialized_;
};

} // namespace Bobot
