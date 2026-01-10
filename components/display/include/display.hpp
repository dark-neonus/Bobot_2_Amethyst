#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <driver/gpio.h>

extern "C" {
#include <u8g2.h>
#include "u8g2_esp32_hal.h"
}

namespace Bobot {

/**
 * @brief Display driver for 2.42" OLED I2C Display (SSD1309)
 * 
 * This class provides a C++ interface for the OLED display
 * using the u8g2 library. It handles initialization and provides
 * methods for drawing text and graphics.
 */
class Display {
public:
    /**
     * @brief Construct a new Display object
     * 
     * @param sda_pin I2C SDA pin (default: GPIO21)
     * @param scl_pin I2C SCL pin (default: GPIO22)
     * @param i2c_address Display I2C address (default: 0x78)
     */
    Display(gpio_num_t sda_pin = GPIO_NUM_21, 
            gpio_num_t scl_pin = GPIO_NUM_22,
            uint8_t i2c_address = 0x78);
    
    /**
     * @brief Destroy the Display object
     */
    ~Display();

    /**
     * @brief Initialize the display
     * 
     * @return true if initialization succeeded
     * @return false if initialization failed
     */
    bool init();

    /**
     * @brief Clear the display buffer
     */
    void clear();

    /**
     * @brief Send buffer to display
     */
    void update();

    /**
     * @brief Draw a string on the display
     * 
     * @param x X coordinate
     * @param y Y coordinate
     * @param text Text to draw
     */
    void drawString(int x, int y, const char* text);

    /**
     * @brief Draw a box on the display
     * 
     * @param x X coordinate
     * @param y Y coordinate
     * @param w Width
     * @param h Height
     */
    void drawBox(int x, int y, int w, int h);

    /**
     * @brief Draw a frame on the display
     * 
     * @param x X coordinate
     * @param y Y coordinate
     * @param w Width
     * @param h Height
     */
    void drawFrame(int x, int y, int w, int h);

    /**
     * @brief Set font for text rendering
     * 
     * @param font Pointer to u8g2 font
     */
    void setFont(const uint8_t* font);

    /**
     * @brief Set power save mode
     * 
     * @param enable true to enable power save, false to disable
     */
    void setPowerSave(bool enable);

    /**
     * @brief Get the underlying u8g2 object
     * 
     * @return u8g2_t* Pointer to u8g2 structure
     */
    u8g2_t* getU8g2Handle();

private:
    u8g2_t u8g2_;
    u8g2_esp32_hal_t hal_;
    uint8_t i2c_address_;
    bool initialized_;
};

} // namespace Bobot

#endif // DISPLAY_HPP
