#ifndef SD_CARD_HPP
#define SD_CARD_HPP

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <string>

extern "C" {
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
}

namespace Bobot {

/**
 * @brief SD Card driver for microSD card via SPI
 * 
 * This class provides a C++ interface for the ESP-IDF SD card driver
 * using SPI mode. It handles mounting/unmounting and provides easy
 * file operations through the VFS (Virtual File System).
 */
class SDCard {
public:
    /**
     * @brief Construct a new SDCard object
     * 
     * @param mosi_pin SPI MOSI pin (default: GPIO23)
     * @param miso_pin SPI MISO pin (default: GPIO19)
     * @param clk_pin SPI CLK pin (default: GPIO18)
     * @param cs_pin SPI CS pin (default: GPIO5)
     * @param mount_point Mount point path (default: "/sdcard")
     * @param max_files Maximum number of open files (default: 5)
     */
    SDCard(gpio_num_t mosi_pin = GPIO_NUM_23,
           gpio_num_t miso_pin = GPIO_NUM_19,
           gpio_num_t clk_pin = GPIO_NUM_18,
           gpio_num_t cs_pin = GPIO_NUM_5,
           const char* mount_point = "/sdcard",
           size_t max_files = 5);
    
    /**
     * @brief Destroy the SDCard object
     */
    ~SDCard();

    /**
     * @brief Initialize and mount the SD card
     * 
     * @param format_if_failed Format card if mount fails (default: false)
     * @return true if mount succeeded
     * @return false if mount failed
     */
    bool mount(bool format_if_failed = false);

    /**
     * @brief Unmount the SD card
     */
    void unmount();

    /**
     * @brief Check if SD card is mounted
     * 
     * @return true if mounted
     * @return false if not mounted
     */
    bool isMounted() const;

    /**
     * @brief Get SD card information
     * 
     * @param name Card name output
     * @param type Card type output
     * @param speed Card speed output (in kHz)
     * @param size_mb Card size output (in MB)
     * @return true if info retrieved successfully
     * @return false if failed
     */
    bool getCardInfo(std::string& name, std::string& type, uint32_t& speed, uint64_t& size_mb);

    /**
     * @brief Get free space on SD card
     * 
     * @return uint64_t Free space in bytes, 0 if failed
     */
    uint64_t getFreeSpace();

    /**
     * @brief Get total space on SD card
     * 
     * @return uint64_t Total space in bytes, 0 if failed
     */
    uint64_t getTotalSpace();

    /**
     * @brief Get mount point path
     * 
     * @return const char* Mount point path
     */
    const char* getMountPoint() const;

    /**
     * @brief Write data to a file
     * 
     * @param filename Filename (relative to mount point)
     * @param data Data to write
     * @param size Size of data in bytes
     * @param append Append to file if true, overwrite if false
     * @return true if write succeeded
     * @return false if write failed
     */
    bool writeFile(const char* filename, const void* data, size_t size, bool append = false);

    /**
     * @brief Read data from a file
     * 
     * @param filename Filename (relative to mount point)
     * @param buffer Buffer to read into
     * @param size Maximum size to read
     * @return size_t Number of bytes actually read, 0 if failed
     */
    size_t readFile(const char* filename, void* buffer, size_t size);

    /**
     * @brief Check if file exists
     * 
     * @param filename Filename (relative to mount point)
     * @return true if file exists
     * @return false if file does not exist
     */
    bool fileExists(const char* filename);

    /**
     * @brief Delete a file
     * 
     * @param filename Filename (relative to mount point)
     * @return true if deletion succeeded
     * @return false if deletion failed
     */
    bool deleteFile(const char* filename);

private:
    gpio_num_t mosi_pin_;
    gpio_num_t miso_pin_;
    gpio_num_t clk_pin_;
    gpio_num_t cs_pin_;
    std::string mount_point_;
    size_t max_files_;
    bool mounted_;
    sdmmc_card_t* card_;
    spi_host_device_t spi_host_;
};

} // namespace Bobot

#endif // SD_CARD_HPP
