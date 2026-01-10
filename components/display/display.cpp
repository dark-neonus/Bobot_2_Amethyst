#include "display.hpp"
#include <esp_log.h>
#include <string.h>

extern "C" {
#include "u8g2_esp32_hal.h"
}

static const char* TAG = "Display";

namespace Bobot {

Display::Display(gpio_num_t sda_pin, gpio_num_t scl_pin, uint8_t i2c_address)
    : i2c_address_(i2c_address), initialized_(false) {
    
    // Initialize HAL structure
    hal_ = U8G2_ESP32_HAL_DEFAULT;
    hal_.bus.i2c.sda = sda_pin;
    hal_.bus.i2c.scl = scl_pin;
}

Display::~Display() {
    // Cleanup if needed
}

bool Display::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "Display already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing display HAL");
    u8g2_esp32_hal_init(hal_);

    ESP_LOGI(TAG, "Setting up u8g2 for SSD1309");
    u8g2_Setup_ssd1309_i2c_128x64_noname2_f(
        &u8g2_, 
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );

    ESP_LOGI(TAG, "Setting I2C address to 0x%02X", i2c_address_);
    u8x8_SetI2CAddress(&u8g2_.u8x8, i2c_address_);

    ESP_LOGI(TAG, "Initializing display");
    u8g2_InitDisplay(&u8g2_);

    ESP_LOGI(TAG, "Waking up display");
    u8g2_SetPowerSave(&u8g2_, 0);

    initialized_ = true;
    ESP_LOGI(TAG, "Display initialization complete");
    
    return true;
}

void Display::clear() {
    u8g2_ClearBuffer(&u8g2_);
}

void Display::update() {
    u8g2_SendBuffer(&u8g2_);
}

void Display::drawString(int x, int y, const char* text) {
    u8g2_DrawStr(&u8g2_, x, y, text);
}

void Display::drawBox(int x, int y, int w, int h) {
    u8g2_DrawBox(&u8g2_, x, y, w, h);
}

void Display::drawFrame(int x, int y, int w, int h) {
    u8g2_DrawFrame(&u8g2_, x, y, w, h);
}

void Display::setFont(const uint8_t* font) {
    u8g2_SetFont(&u8g2_, font);
}

void Display::setPowerSave(bool enable) {
    u8g2_SetPowerSave(&u8g2_, enable ? 1 : 0);
}

u8g2_t* Display::getU8g2Handle() {
    return &u8g2_;
}

} // namespace Bobot
