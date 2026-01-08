#include "bmi160.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cmath>

static const char* TAG = "BMI160";

// BMI160 Register Map
#define BMI160_REG_CHIP_ID          0x00
#define BMI160_REG_ERR_REG          0x02
#define BMI160_REG_PMU_STATUS       0x03
#define BMI160_REG_DATA_0           0x04  // Start of sensor data (MAG)
#define BMI160_REG_DATA_8           0x0C  // Gyro X LSB
#define BMI160_REG_DATA_14          0x12  // Accel X LSB
#define BMI160_REG_SENSORTIME_0     0x18
#define BMI160_REG_STATUS           0x1B
#define BMI160_REG_INT_STATUS_0     0x1C
#define BMI160_REG_TEMPERATURE_0    0x20
#define BMI160_REG_ACC_CONF         0x40
#define BMI160_REG_ACC_RANGE        0x41
#define BMI160_REG_GYR_CONF         0x42
#define BMI160_REG_GYR_RANGE        0x43
#define BMI160_REG_FIFO_CONFIG_0    0x46
#define BMI160_REG_FIFO_CONFIG_1    0x47
#define BMI160_REG_INT_EN_0         0x50
#define BMI160_REG_INT_EN_1         0x51
#define BMI160_REG_INT_EN_2         0x52
#define BMI160_REG_INT_OUT_CTRL     0x53
#define BMI160_REG_INT_LATCH        0x54
#define BMI160_REG_INT_MAP_0        0x55
#define BMI160_REG_INT_MAP_1        0x56
#define BMI160_REG_INT_MAP_2        0x57
#define BMI160_REG_INT_MOTION_0     0x5F  // Any-motion threshold
#define BMI160_REG_INT_MOTION_1     0x60  // Any-motion duration
#define BMI160_REG_INT_MOTION_2     0x61  // Any-motion config
#define BMI160_REG_INT_MOTION_3     0x62  // No-motion threshold
#define BMI160_REG_INT_TAP_0        0x63
#define BMI160_REG_INT_TAP_1        0x64
#define BMI160_REG_CMD              0x7E

// Commands
#define BMI160_CMD_SOFT_RESET       0xB6
#define BMI160_CMD_ACC_SET_PMU_MODE 0x11  // Normal mode
#define BMI160_CMD_GYR_SET_PMU_MODE 0x15  // Normal mode

// Chip ID
#define BMI160_CHIP_ID              0xD1

// Timeout for I2C operations
#define BMI160_I2C_TIMEOUT_MS       100

namespace Bobot {

BMI160::BMI160(i2c_master_bus_handle_t bus, gpio_num_t int1_pin, uint8_t address)
    : m_bus_handle(bus)
    , m_dev_handle(nullptr)
    , m_address(address)
    , m_int1_pin(int1_pin)
    , m_accel_scale(0.0f)
    , m_gyro_scale(0.0f)
{
}

BMI160::~BMI160()
{
    if (m_dev_handle) {
        i2c_master_bus_rm_device(m_dev_handle);
    }
}

bool BMI160::init()
{
    ESP_LOGI(TAG, "Initializing BMI160 at I2C address 0x%02X, INT1 on GPIO%d", m_address, m_int1_pin);

    // Add device to I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = m_address,
        .scl_speed_hz = 400000,  // 400kHz I2C
        .scl_wait_us = 0,
        .flags = {},
    };

    esp_err_t ret = i2c_master_bus_add_device(m_bus_handle, &dev_cfg, &m_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add BMI160 to I2C bus: %s", esp_err_to_name(ret));
        return false;
    }

    // Soft reset
    if (!softReset()) {
        ESP_LOGE(TAG, "Soft reset failed");
        return false;
    }

    // Verify chip ID
    uint8_t chip_id = 0;
    if (!readChipId(chip_id)) {
        ESP_LOGE(TAG, "Failed to read chip ID");
        return false;
    }

    if (chip_id != BMI160_CHIP_ID) {
        ESP_LOGE(TAG, "Invalid chip ID: 0x%02X (expected 0xD1)", chip_id);
        return false;
    }

    ESP_LOGI(TAG, "BMI160 chip ID verified: 0x%02X", chip_id);

    // Set power mode (normal for both accel and gyro)
    if (!setPowerMode()) {
        ESP_LOGE(TAG, "Failed to set power mode");
        return false;
    }

    // Configure accelerometer
    if (!configureAccel()) {
        ESP_LOGE(TAG, "Failed to configure accelerometer");
        return false;
    }

    // Configure gyroscope
    if (!configureGyro()) {
        ESP_LOGE(TAG, "Failed to configure gyroscope");
        return false;
    }

    // Configure data-ready interrupt (fires every new sample at 100Hz)
    if (!configureInterrupt(InterruptEvent::DATA_READY)) {
        ESP_LOGE(TAG, "Failed to configure interrupt");
        return false;
    }

    ESP_LOGI(TAG, "BMI160 initialization complete");
    return true;
}

bool BMI160::configureInterrupt(InterruptEvent event, bool int1, IntPinMode mode, IntPinLevel level)
{
    ESP_LOGI(TAG, "Configuring interrupt event: %d on INT%d", static_cast<int>(event), int1 ? 1 : 2);

    // Configure interrupt pin electrical properties (INT_OUT_CTRL register)
    uint8_t int_out_ctrl = 0;
    
    // INT1 configuration
    if (int1) {
        int_out_ctrl |= (mode == IntPinMode::OPEN_DRAIN) ? (1 << 0) : 0;     // INT1 output mode
        int_out_ctrl |= (level == IntPinLevel::ACTIVE_HIGH) ? (1 << 1) : 0;  // INT1 level
    } else {
        // INT2 configuration
        int_out_ctrl |= (mode == IntPinMode::OPEN_DRAIN) ? (1 << 2) : 0;     // INT2 output mode
        int_out_ctrl |= (level == IntPinLevel::ACTIVE_HIGH) ? (1 << 3) : 0;  // INT2 level
    }

    if (!writeRegister(BMI160_REG_INT_OUT_CTRL, int_out_ctrl)) {
        ESP_LOGE(TAG, "Failed to configure interrupt output control");
        return false;
    }

    // Configure interrupt latch mode (non-latched, pulsed)
    if (!writeRegister(BMI160_REG_INT_LATCH, 0x00)) {
        ESP_LOGE(TAG, "Failed to configure interrupt latch");
        return false;
    }

    // Enable and map specific interrupt based on event type
    switch (event) {
        case InterruptEvent::ANY_MOTION:
            return configureAnyMotionInterrupt();

        case InterruptEvent::DATA_READY:
            // Enable data ready interrupt
            if (!writeRegister(BMI160_REG_INT_EN_1, 0x10)) {  // Bit 4: DRDY enable
                return false;
            }
            // Map to INT1 or INT2
            if (!writeRegister(BMI160_REG_INT_MAP_1, int1 ? 0x80 : 0x00)) {
                return false;
            }
            break;

        case InterruptEvent::DOUBLE_TAP:
            // Enable double tap
            if (!writeRegister(BMI160_REG_INT_EN_0, 0x10)) {  // Bit 4: D_TAP enable
                return false;
            }
            // Map to INT1 or INT2
            if (!writeRegister(BMI160_REG_INT_MAP_0, int1 ? 0x10 : 0x00)) {
                return false;
            }
            // Configure tap timing
            if (!writeRegister(BMI160_REG_INT_TAP_0, 0x04)) {  // Tap threshold
                return false;
            }
            if (!writeRegister(BMI160_REG_INT_TAP_1, 0x03)) {  // Tap timing
                return false;
            }
            break;

        case InterruptEvent::SINGLE_TAP:
            // Enable single tap
            if (!writeRegister(BMI160_REG_INT_EN_0, 0x20)) {  // Bit 5: S_TAP enable
                return false;
            }
            // Map to INT1 or INT2
            if (!writeRegister(BMI160_REG_INT_MAP_0, int1 ? 0x20 : 0x00)) {
                return false;
            }
            // Configure tap timing
            if (!writeRegister(BMI160_REG_INT_TAP_0, 0x04)) {
                return false;
            }
            if (!writeRegister(BMI160_REG_INT_TAP_1, 0x03)) {
                return false;
            }
            break;

        case InterruptEvent::NO_MOTION:
            // Enable no-motion interrupt
            if (!writeRegister(BMI160_REG_INT_EN_2, 0x08)) {  // Bit 3: NOMOTION enable
                return false;
            }
            // Map to INT1 or INT2
            if (!writeRegister(BMI160_REG_INT_MAP_0, int1 ? 0x08 : 0x00)) {
                return false;
            }
            // Configure no-motion threshold and duration
            if (!writeRegister(BMI160_REG_INT_MOTION_3, 0x14)) {  // Threshold
                return false;
            }
            if (!writeRegister(BMI160_REG_INT_MOTION_2, 0x05)) {  // Duration
                return false;
            }
            break;

        case InterruptEvent::STEP_DETECTOR:
            // Enable step detector
            if (!writeRegister(BMI160_REG_INT_EN_2, 0x08)) {  // Bit 3: STEP_INT enable
                return false;
            }
            // Map to INT1 or INT2
            if (!writeRegister(BMI160_REG_INT_MAP_0, int1 ? 0x01 : 0x00)) {
                return false;
            }
            break;

        default:
            ESP_LOGW(TAG, "Interrupt event not fully implemented: %d", static_cast<int>(event));
            return false;
    }

    ESP_LOGI(TAG, "Interrupt configuration complete");
    return true;
}

bool BMI160::readAccel(AccelData& data)
{
    uint8_t raw_data[6];
    
    // Read 6 bytes starting from DATA_14 (accel X LSB at 0x12)
    if (!readRegisters(BMI160_REG_DATA_14, raw_data, 6)) {
        ESP_LOGE(TAG, "Failed to read accelerometer data");
        return false;
    }

    // Combine LSB and MSB for each axis (little-endian)
    int16_t accel_x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
    int16_t accel_y = (int16_t)((raw_data[3] << 8) | raw_data[2]);
    int16_t accel_z = (int16_t)((raw_data[5] << 8) | raw_data[4]);

    // Convert to m/s² using scale factor
    data.x = accel_x * m_accel_scale;
    data.y = accel_y * m_accel_scale;
    data.z = accel_z * m_accel_scale;

    return true;
}

bool BMI160::readGyro(GyroData& data)
{
    uint8_t raw_data[6];
    
    // Read 6 bytes starting from DATA_8 (gyro X LSB)
    if (!readRegisters(BMI160_REG_DATA_8, raw_data, 6)) {
        ESP_LOGE(TAG, "Failed to read gyroscope data");
        return false;
    }

    // Combine LSB and MSB for each axis (little-endian)
    int16_t gyro_x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
    int16_t gyro_y = (int16_t)((raw_data[3] << 8) | raw_data[2]);
    int16_t gyro_z = (int16_t)((raw_data[5] << 8) | raw_data[4]);

    // Convert to rad/s using scale factor
    data.x = gyro_x * m_gyro_scale;
    data.y = gyro_y * m_gyro_scale;
    data.z = gyro_z * m_gyro_scale;

    return true;
}

bool BMI160::readChipId(uint8_t& chip_id)
{
    return readRegister(BMI160_REG_CHIP_ID, chip_id);
}

bool BMI160::writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t write_buf[2] = {reg, value};
    esp_err_t ret = i2c_master_transmit(m_dev_handle, write_buf, 2, BMI160_I2C_TIMEOUT_MS);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02X: %s", reg, esp_err_to_name(ret));
        return false;
    }
    
    // Small delay for register write to take effect
    vTaskDelay(pdMS_TO_TICKS(1));
    return true;
}

bool BMI160::readRegister(uint8_t reg, uint8_t& value)
{
    esp_err_t ret = i2c_master_transmit_receive(m_dev_handle, &reg, 1, &value, 1, BMI160_I2C_TIMEOUT_MS);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool BMI160::readRegisters(uint8_t reg, uint8_t* data, size_t length)
{
    esp_err_t ret = i2c_master_transmit_receive(m_dev_handle, &reg, 1, data, length, BMI160_I2C_TIMEOUT_MS);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read %d registers from 0x%02X: %s", length, reg, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool BMI160::softReset()
{
    ESP_LOGI(TAG, "Performing soft reset");
    
    if (!writeRegister(BMI160_REG_CMD, BMI160_CMD_SOFT_RESET)) {
        return false;
    }
    
    // Wait for reset to complete (datasheet: 1-2ms typical)
    vTaskDelay(pdMS_TO_TICKS(10));
    return true;
}

bool BMI160::setPowerMode()
{
    ESP_LOGI(TAG, "Setting power mode to normal");
    
    // Set accelerometer to normal mode
    if (!writeRegister(BMI160_REG_CMD, BMI160_CMD_ACC_SET_PMU_MODE)) {
        ESP_LOGE(TAG, "Failed to set accelerometer power mode");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // Accel startup time: min 3.8ms, use 10ms for safety
    
    // Verify accelerometer power mode
    uint8_t pmu_status = 0;
    if (readRegister(BMI160_REG_PMU_STATUS, pmu_status)) {
        uint8_t accel_pmu = (pmu_status >> 4) & 0x03;
        ESP_LOGI(TAG, "After accel command, PMU status: 0x%02X (accel: %d)", pmu_status, accel_pmu);
    }
    
    // Set gyroscope to normal mode
    if (!writeRegister(BMI160_REG_CMD, BMI160_CMD_GYR_SET_PMU_MODE)) {
        ESP_LOGE(TAG, "Failed to set gyroscope power mode");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // Gyro startup time: min 80ms, use 100ms for safety
    
    // Verify final PMU status
    if (!readRegister(BMI160_REG_PMU_STATUS, pmu_status)) {
        ESP_LOGE(TAG, "Failed to read PMU status");
        return false;
    }
    
    ESP_LOGI(TAG, "Final PMU status: 0x%02X", pmu_status);
    
    // Check if both accel and gyro are in normal mode
    // Bits [5:4] = accel PMU, bits [3:2] = gyro PMU
    // 01 = normal mode for both
    uint8_t accel_pmu = (pmu_status >> 4) & 0x03;
    uint8_t gyro_pmu = (pmu_status >> 2) & 0x03;
    
    ESP_LOGI(TAG, "Power modes - Accel: %d (expect 1), Gyro: %d (expect 1)", accel_pmu, gyro_pmu);
    
    if (accel_pmu != 0x01) {
        ESP_LOGE(TAG, "Accelerometer not in normal mode: %d", accel_pmu);
        return false;
    }
    
    if (gyro_pmu != 0x01) {
        ESP_LOGE(TAG, "Gyroscope not in normal mode: %d", gyro_pmu);
        ESP_LOGI(TAG, "Attempting gyroscope power-up again...");
        
        // Try one more time with longer delay
        if (!writeRegister(BMI160_REG_CMD, BMI160_CMD_GYR_SET_PMU_MODE)) {
            ESP_LOGE(TAG, "Failed to send second gyroscope power command");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(150));
        
        if (!readRegister(BMI160_REG_PMU_STATUS, pmu_status)) {
            ESP_LOGE(TAG, "Failed to read PMU status after retry");
            return false;
        }
        
        gyro_pmu = (pmu_status >> 2) & 0x03;
        ESP_LOGI(TAG, "After retry, PMU status: 0x%02X (gyro: %d)", pmu_status, gyro_pmu);
        
        if (gyro_pmu != 0x01) {
            ESP_LOGE(TAG, "Gyroscope still not in normal mode after retry");
            return false;
        }
    }
    
    ESP_LOGI(TAG, "Both sensors in normal mode");
    return true;
}

bool BMI160::configureAccel()
{
    ESP_LOGI(TAG, "Configuring accelerometer");
    
    // Configure accelerometer:
    // - ODR: 100Hz (0x08)
    // - Bandwidth: Normal (OSR4) (0x02)
    // - Undersampling: disabled (0x00)
    // Register value: 0bXXXX10XX = 0x28 (100Hz, Normal)
    if (!writeRegister(BMI160_REG_ACC_CONF, 0x28)) {
        ESP_LOGE(TAG, "Failed to configure accelerometer ODR/bandwidth");
        return false;
    }
    
    // Set accelerometer range to ±2g (0x03)
    if (!writeRegister(BMI160_REG_ACC_RANGE, 0x03)) {
        ESP_LOGE(TAG, "Failed to set accelerometer range");
        return false;
    }
    
    // Calculate scale factor for ±2g range
    // 16-bit signed: ±32768 = ±2g
    // 1 LSB = 2g / 32768 = 0.00006103515625 g = 0.000598550415 m/s²
    m_accel_scale = (2.0f * 9.80665f) / 32768.0f;  // Convert to m/s²
    
    ESP_LOGI(TAG, "Accelerometer configured: ±2g range, 100Hz ODR, scale=%.9f m/s²/LSB", m_accel_scale);
    return true;
}

bool BMI160::configureGyro()
{
    ESP_LOGI(TAG, "Configuring gyroscope");
    
    // Configure gyroscope:
    // - ODR: 100Hz (0x08)
    // - Bandwidth: Normal (0x02)
    // Register value: 0bXXXX10XX = 0x28 (100Hz, Normal)
    if (!writeRegister(BMI160_REG_GYR_CONF, 0x28)) {
        ESP_LOGE(TAG, "Failed to configure gyroscope ODR/bandwidth");
        return false;
    }
    
    // Set gyroscope range to ±250°/s (0x03)
    if (!writeRegister(BMI160_REG_GYR_RANGE, 0x03)) {
        ESP_LOGE(TAG, "Failed to set gyroscope range");
        return false;
    }
    
    // Calculate scale factor for ±250°/s range
    // 16-bit signed: ±32768 = ±250°/s
    // 1 LSB = 250°/s / 32768 = 0.007629394531 °/s
    // Convert to rad/s: * (π/180)
    const float deg_to_rad = M_PI / 180.0f;
    m_gyro_scale = (250.0f / 32768.0f) * deg_to_rad;  // Convert to rad/s
    
    ESP_LOGI(TAG, "Gyroscope configured: ±250°/s range, 100Hz ODR, scale=%.9f rad/s/LSB", m_gyro_scale);
    return true;
}

bool BMI160::configureAnyMotionInterrupt()
{
    ESP_LOGI(TAG, "Configuring any-motion interrupt");
    
    // Set any-motion threshold (register 0x5F)
    // Threshold = value * 3.91mg (for ±2g range)
    // Example: 0x14 (20) = 78.2mg = 0.767 m/s² threshold
    if (!writeRegister(BMI160_REG_INT_MOTION_0, 0x14)) {
        ESP_LOGE(TAG, "Failed to set any-motion threshold");
        return false;
    }
    
    // Set any-motion duration (register 0x60)
    // Duration = (value + 1) * 20ms (at 100Hz ODR)
    // Example: 0x01 = 2 samples = 40ms
    if (!writeRegister(BMI160_REG_INT_MOTION_1, 0x01)) {
        ESP_LOGE(TAG, "Failed to set any-motion duration");
        return false;
    }
    
    // Enable any-motion interrupt on all axes (register 0x50)
    // Bit 0: X-axis enable
    // Bit 1: Y-axis enable
    // Bit 2: Z-axis enable
    if (!writeRegister(BMI160_REG_INT_EN_0, 0x07)) {
        ESP_LOGE(TAG, "Failed to enable any-motion interrupt");
        return false;
    }
    
    // Map any-motion interrupt to INT1 pin (register 0x55)
    // Bit 2: ANYMOTION to INT1
    if (!writeRegister(BMI160_REG_INT_MAP_0, 0x04)) {
        ESP_LOGE(TAG, "Failed to map any-motion to INT1");
        return false;
    }
    
    ESP_LOGI(TAG, "Any-motion interrupt configured: threshold=78.2mg, duration=40ms");
    return true;
}

} // namespace Bobot
