#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "driver/gpio.h"

namespace Bobot {

/**
 * @brief BMI160 6-axis IMU (gyroscope + accelerometer) driver
 * 
 * Features:
 * - I2C communication using new i2c_master API
 * - Interrupt-driven data acquisition (non-blocking ISR)
 * - Configurable interrupt events (any-motion, no-motion, tap, etc.)
 * - Default: any-motion interrupt on INT1 pin
 * 
 * Typical usage:
 * 1. Create instance with shared I2C bus handle and INT1 GPIO
 * 2. Call init() to configure sensor
 * 3. Setup GPIO interrupt handler
 * 4. When interrupt flag is set, call readAccel() and readGyro()
 */
class BMI160 {
public:
    // Interrupt event types
    enum class InterruptEvent {
        ANY_MOTION,      // Default: wake on movement
        NO_MOTION,       // Sleep when stationary
        DOUBLE_TAP,      // Double tap detection
        SINGLE_TAP,      // Single tap detection
        ORIENTATION,     // Portrait/landscape change
        FLAT,            // Flat detection
        LOW_G,           // Freefall detection
        HIGH_G,          // High G shock detection
        DATA_READY,      // New data available
        FIFO_FULL,       // FIFO buffer full
        FIFO_WATERMARK,  // FIFO watermark reached
        STEP_DETECTOR    // Step detected (pedometer)
    };

    // Accelerometer data structure (in m/s²)
    struct AccelData {
        float x;
        float y;
        float z;
    };

    // Gyroscope data structure (in rad/s)
    struct GyroData {
        float x;
        float y;
        float z;
    };

    // Interrupt pin configuration
    enum class IntPinMode {
        PUSH_PULL,
        OPEN_DRAIN
    };

    enum class IntPinLevel {
        ACTIVE_LOW,
        ACTIVE_HIGH
    };

    /**
     * @brief Construct BMI160 driver
     * 
     * @param bus Shared I2C bus handle (GPIO21 SDA, GPIO22 SCL)
     * @param int1_pin GPIO pin for INT1 interrupt (suggested: GPIO4)
     * @param address I2C address (default: 0x68, alt: 0x69)
     */
    BMI160(i2c_master_bus_handle_t bus, gpio_num_t int1_pin, uint8_t address = 0x68);
    ~BMI160();

    /**
     * @brief Initialize BMI160 sensor
     * 
     * - Performs soft reset
     * - Configures accelerometer (±2g range, 100Hz ODR)
     * - Configures gyroscope (±250°/s range, 100Hz ODR)
     * - Sets up any-motion interrupt on INT1 (default)
     * 
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Configure interrupt event type
     * 
     * @param event Interrupt event to enable
     * @param int1 Use INT1 pin (true) or INT2 pin (false)
     * @param mode Push-pull or open-drain
     * @param level Active high or active low
     * @return true if configuration successful
     */
    bool configureInterrupt(InterruptEvent event, 
                          bool int1 = true,
                          IntPinMode mode = IntPinMode::PUSH_PULL,
                          IntPinLevel level = IntPinLevel::ACTIVE_HIGH);

    /**
     * @brief Read accelerometer data
     * 
     * Should be called when interrupt flag is set.
     * Reads 6 bytes from sensor and converts to m/s².
     * 
     * @param data Output accelerometer data
     * @return true if read successful
     */
    bool readAccel(AccelData& data);

    /**
     * @brief Read gyroscope data
     * 
     * Should be called when interrupt flag is set.
     * Reads 6 bytes from sensor and converts to rad/s.
     * 
     * @param data Output gyroscope data
     * @return true if read successful
     */
    bool readGyro(GyroData& data);

    /**
     * @brief Read chip ID (should return 0xD1)
     * 
     * @param chip_id Output chip ID
     * @return true if read successful
     */
    bool readChipId(uint8_t& chip_id);

    /**
     * @brief Get INT1 GPIO pin number
     */
    gpio_num_t getInt1Pin() const { return m_int1_pin; }

private:
    // I2C communication
    i2c_master_bus_handle_t m_bus_handle;
    i2c_master_dev_handle_t m_dev_handle;
    uint8_t m_address;
    gpio_num_t m_int1_pin;

    // Register read/write helpers
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t& value);
    bool readRegisters(uint8_t reg, uint8_t* data, size_t length);

    // Initialization helpers
    bool softReset();
    bool setPowerMode();
    bool configureAccel();
    bool configureGyro();
    bool configureAnyMotionInterrupt();

    // Scale factors for data conversion
    float m_accel_scale;  // LSB to m/s² conversion
    float m_gyro_scale;   // LSB to rad/s conversion
};

} // namespace Bobot
