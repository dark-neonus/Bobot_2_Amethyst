/**
 * @file buzzer.cpp
 * @brief PWM-controlled buzzer driver implementation
 */

#include "buzzer.hpp"
#include <driver/ledc.h>
#include <esp_log.h>

static const char* TAG = "Buzzer";

namespace Bobot {

Buzzer::Buzzer()
    : duty_percent_(50),
      is_on_(false),
      initialized_(false) {
}

Buzzer::~Buzzer() {
    if (initialized_) {
        off();
    }
}

esp_err_t Buzzer::init(const Config& config) {
    if (initialized_) {
        ESP_LOGW(TAG, "Buzzer already initialized");
        return ESP_OK;
    }

    config_ = config;

    ESP_LOGI(TAG, "Initializing buzzer on GPIO%d", config_.pin);

    // Configure LEDC timer
    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = config_.mode;
    timer_config.duty_resolution = LEDC_TIMER_8_BIT;  // 8-bit resolution (0-255)
    timer_config.timer_num = config_.timer;
    timer_config.freq_hz = config_.frequency;
    timer_config.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel
    ledc_channel_config_t channel_config = {};
    channel_config.speed_mode = config_.mode;
    channel_config.channel = config_.channel;
    channel_config.timer_sel = config_.timer;
    channel_config.gpio_num = config_.pin;
    channel_config.duty = 0;  // Start with duty cycle 0 (off)
    channel_config.hpoint = 0;

    ret = ledc_channel_config(&channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Buzzer initialized successfully");

    return ESP_OK;
}

void Buzzer::on() {
    if (!initialized_) {
        ESP_LOGE(TAG, "Buzzer not initialized");
        return;
    }

    // Convert percentage to 8-bit duty value (0-255)
    uint32_t duty_value = (duty_percent_ * 255) / 100;

    esp_err_t ret = ledc_set_duty(config_.mode, config_.channel, duty_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty: %s", esp_err_to_name(ret));
        return;
    }

    ret = ledc_update_duty(config_.mode, config_.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty: %s", esp_err_to_name(ret));
        return;
    }

    is_on_ = true;
    ESP_LOGD(TAG, "Buzzer turned on with duty cycle %d%%", duty_percent_);
}

void Buzzer::off() {
    if (!initialized_) {
        ESP_LOGE(TAG, "Buzzer not initialized");
        return;
    }

    esp_err_t ret = ledc_set_duty(config_.mode, config_.channel, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty to 0: %s", esp_err_to_name(ret));
        return;
    }

    ret = ledc_update_duty(config_.mode, config_.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty: %s", esp_err_to_name(ret));
        return;
    }

    is_on_ = false;
    ESP_LOGD(TAG, "Buzzer turned off");
}

void Buzzer::setDutyCycle(uint8_t duty_percent) {
    if (duty_percent > 100) {
        duty_percent = 100;
    }

    duty_percent_ = duty_percent;
    ESP_LOGD(TAG, "Duty cycle set to %d%%", duty_percent_);

    // If buzzer is currently on, update the duty cycle immediately
    if (is_on_) {
        on();
    }
}

uint8_t Buzzer::getDutyCycle() const {
    return duty_percent_;
}

bool Buzzer::isOn() const {
    return is_on_;
}

void Buzzer::toggle() {
    if (is_on_) {
        off();
    } else {
        on();
    }
}

} // namespace Bobot
