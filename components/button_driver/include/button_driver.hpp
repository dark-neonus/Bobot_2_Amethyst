#ifndef BUTTON_DRIVER_HPP
#define BUTTON_DRIVER_HPP

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <cstdint>
#include <functional>

namespace Bobot {

/**
 * @brief Button identifiers for the 9 back panel buttons
 */
enum class Button : uint8_t {
    BACK = 0,      // GPA0
    UP = 1,        // GPA1
    UI = 2,        // GPA2
    LEFT = 3,      // GPA3
    OK = 4,        // GPA4
    RIGHT = 5,     // GPA5
    SETTINGS = 6,  // GPA6
    DOWN = 7,      // GPA7
    DEBUG = 8      // GPB0
};

/**
 * @brief Button event types
 */
enum class ButtonEvent {
    PRESSED,
    RELEASED
};

/**
 * @brief Button callback function type
 */
using ButtonCallback = std::function<void(Button button, ButtonEvent event)>;

/**
 * @brief Driver for MCP23017 I/O Expander controlling 9 buttons
 * 
 * This class provides a C++ interface for the MCP23017 16-bit I/O expander
 * connected via I2C. It handles button input reading, debouncing, and
 * interrupt-driven event detection.
 */
class ButtonDriver {
public:
    /**
     * @brief Construct a new Button Driver object
     * 
     * @param bus_handle I2C master bus handle
     * @param i2c_address MCP23017 I2C address (default: 0x20)
     * @param inta_pin Interrupt A pin (default: GPIO_NUM_NC - not connected)
     * @param intb_pin Interrupt B pin (default: GPIO_NUM_NC - not connected)
     */
    ButtonDriver(i2c_master_bus_handle_t bus_handle,
                 uint8_t i2c_address = 0x20,
                 gpio_num_t inta_pin = GPIO_NUM_NC,
                 gpio_num_t intb_pin = GPIO_NUM_NC);
    
    /**
     * @brief Destroy the Button Driver object
     */
    ~ButtonDriver();

    /**
     * @brief Initialize the button driver
     * 
     * @return true if initialization succeeded
     * @return false if initialization failed
     */
    bool init();

    /**
     * @brief Read the current state of all buttons
     * 
     * @param button_states Output array of 9 button states (true = pressed)
     * @return true if read succeeded
     * @return false if read failed
     */
    bool readButtons(bool* button_states);

    /**
     * @brief Check if a specific button is pressed
     * 
     * @param button Button to check
     * @return true if button is pressed
     * @return false if button is not pressed
     */
    bool isButtonPressed(Button button);

    /**
     * @brief Set callback function for button events
     * 
     * @param callback Function to call on button events
     */
    void setButtonCallback(ButtonCallback callback);

    /**
     * @brief Start polling task for button state changes
     * 
     * @param poll_rate_ms Polling rate in milliseconds (default: 50ms)
     * @return true if task started successfully
     * @return false if task start failed
     */
    bool startPolling(uint32_t poll_rate_ms = 50);

    /**
     * @brief Stop polling task
     */
    void stopPolling();

private:
    // MCP23017 register addresses
    static constexpr uint8_t REG_IODIRA = 0x00;
    static constexpr uint8_t REG_IODIRB = 0x01;
    static constexpr uint8_t REG_IPOLA = 0x02;
    static constexpr uint8_t REG_IPOLB = 0x03;
    static constexpr uint8_t REG_GPINTENA = 0x04;
    static constexpr uint8_t REG_GPINTENB = 0x05;
    static constexpr uint8_t REG_GPPUA = 0x0C;
    static constexpr uint8_t REG_GPPUB = 0x0D;
    static constexpr uint8_t REG_GPIOA = 0x12;
    static constexpr uint8_t REG_GPIOB = 0x13;

    i2c_master_bus_handle_t bus_handle_;
    i2c_master_dev_handle_t dev_handle_;
    uint8_t i2c_address_;
    gpio_num_t inta_pin_;
    gpio_num_t intb_pin_;
    bool initialized_;
    bool polling_;
    uint16_t button_state_;
    ButtonCallback callback_;

    /**
     * @brief Write a byte to MCP23017 register
     */
    bool writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief Read a byte from MCP23017 register
     */
    bool readRegister(uint8_t reg, uint8_t* value);

    /**
     * @brief Polling task function
     */
    static void pollingTask(void* parameter);

    /**
     * @brief Process button state changes
     */
    void processButtonChanges(uint16_t new_state);
};

} // namespace Bobot

#endif // BUTTON_DRIVER_HPP
