#include "button_driver.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "ButtonDriver";

namespace Bobot {

ButtonDriver::ButtonDriver(i2c_master_bus_handle_t bus_handle, uint8_t i2c_address,
                           gpio_num_t inta_pin, gpio_num_t intb_pin)
    : bus_handle_(bus_handle),
      dev_handle_(nullptr),
      i2c_address_(i2c_address),
      inta_pin_(inta_pin),
      intb_pin_(intb_pin),
      initialized_(false),
      polling_(false),
      button_state_(0xFFFF),
      callback_(nullptr) {
}

ButtonDriver::~ButtonDriver() {
    stopPolling();
}

bool ButtonDriver::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "Button driver already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing MCP23017 button driver");
    
    // Add device to I2C bus
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = i2c_address_;
    dev_cfg.scl_speed_hz = 100000;
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle_, &dev_cfg, &dev_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Configure all GPA pins (0-7) as inputs for buttons
    if (!writeRegister(REG_IODIRA, 0xFF)) {
        ESP_LOGE(TAG, "Failed to configure IODIRA");
        return false;
    }

    // Configure GPB0 as input, rest as outputs (not used)
    if (!writeRegister(REG_IODIRB, 0x01)) {
        ESP_LOGE(TAG, "Failed to configure IODIRB");
        return false;
    }

    // Note: External pull-ups are on the PCB, so internal pull-ups not needed
    // But we'll enable them anyway as backup
    if (!writeRegister(REG_GPPUA, 0xFF)) {
        ESP_LOGW(TAG, "Failed to configure pull-ups on GPIOA");
    }
    
    if (!writeRegister(REG_GPPUB, 0x01)) {
        ESP_LOGW(TAG, "Failed to configure pull-ups on GPIOB");
    }

    // Enable interrupt-on-change for all button pins
    if (!writeRegister(REG_GPINTENA, 0xFF)) {
        ESP_LOGW(TAG, "Failed to enable interrupts on GPIOA");
    }
    
    if (!writeRegister(REG_GPINTENB, 0x01)) {
        ESP_LOGW(TAG, "Failed to enable interrupts on GPIOB");
    }

    // Read initial button state
    uint8_t porta, portb;
    if (readRegister(REG_GPIOA, &porta) && readRegister(REG_GPIOB, &portb)) {
        button_state_ = (static_cast<uint16_t>(portb) << 8) | porta;
        ESP_LOGI(TAG, "Initial button state: 0x%04X", button_state_);
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Button driver initialization complete");
    
    return true;
}

bool ButtonDriver::writeRegister(uint8_t reg, uint8_t value) {
    uint8_t write_buf[2] = {reg, value};
    
    esp_err_t ret = i2c_master_transmit(dev_handle_, write_buf, 2, 1000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02X: %s", reg, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool ButtonDriver::readRegister(uint8_t reg, uint8_t* value) {
    esp_err_t ret = i2c_master_transmit_receive(dev_handle_, &reg, 1, value, 1, 1000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool ButtonDriver::readButtons(bool* button_states) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Button driver not initialized");
        return false;
    }

    uint8_t porta, portb;
    
    if (!readRegister(REG_GPIOA, &porta)) {
        return false;
    }
    
    if (!readRegister(REG_GPIOB, &portb)) {
        return false;
    }

    uint16_t state = (static_cast<uint16_t>(portb) << 8) | porta;
    
    // Buttons are active-low (pressed = 0), so invert
    for (int i = 0; i < 9; i++) {
        button_states[i] = !(state & (1 << i));
    }
    
    return true;
}

bool ButtonDriver::isButtonPressed(Button button) {
    if (!initialized_) {
        return false;
    }

    uint8_t btn_idx = static_cast<uint8_t>(button);
    if (btn_idx >= 9) {
        return false;
    }

    // Use cached state from polling task (updated every 10ms)
    // This avoids blocking I2C reads on every call
    uint16_t mask = (1 << btn_idx);
    return !(button_state_ & mask);  // Active-low
}

void ButtonDriver::setButtonCallback(ButtonCallback callback) {
    callback_ = callback;
}

bool ButtonDriver::startPolling(uint32_t poll_rate_ms) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Button driver not initialized");
        return false;
    }

    if (polling_) {
        ESP_LOGW(TAG, "Polling already started");
        return true;
    }

    polling_ = true;
    
    // Create polling task
    BaseType_t ret = xTaskCreate(
        pollingTask,
        "button_poll",
        2048,
        this,
        5,
        nullptr
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create polling task");
        polling_ = false;
        return false;
    }

    ESP_LOGI(TAG, "Button polling started (rate: %lu ms)", poll_rate_ms);
    return true;
}

void ButtonDriver::stopPolling() {
    polling_ = false;
}

void ButtonDriver::pollingTask(void* parameter) {
    ButtonDriver* driver = static_cast<ButtonDriver*>(parameter);
    
    while (driver->polling_) {
        uint8_t porta, portb;
        
        if (driver->readRegister(REG_GPIOA, &porta) && 
            driver->readRegister(REG_GPIOB, &portb)) {
            
            uint16_t new_state = (static_cast<uint16_t>(portb) << 8) | porta;
            
            if (new_state != driver->button_state_) {
                driver->processButtonChanges(new_state);
                driver->button_state_ = new_state;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(nullptr);
}

void ButtonDriver::processButtonChanges(uint16_t new_state) {
    if (!callback_) {
        return;
    }

    // Check each of the 9 buttons
    for (int i = 0; i < 9; i++) {
        uint16_t mask = (1 << i);
        bool old_pressed = !(button_state_ & mask);  // Active-low
        bool new_pressed = !(new_state & mask);      // Active-low
        
        if (old_pressed != new_pressed) {
            Button button = static_cast<Button>(i);
            ButtonEvent event = new_pressed ? ButtonEvent::PRESSED : ButtonEvent::RELEASED;
            
            ESP_LOGI(TAG, "Button %d %s", i, new_pressed ? "pressed" : "released");
            callback_(button, event);
        }
    }
}

} // namespace Bobot
