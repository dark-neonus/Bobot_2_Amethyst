#include "sd_card.hpp"
#include <esp_log.h>
#include <driver/gpio.h>
#include <sys/stat.h>
#include <sys/unistd.h>

extern "C" {
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
}

static const char* TAG = "SDCard";

namespace Bobot {

SDCard::SDCard(gpio_num_t cmd_pin, gpio_num_t dat0_pin, gpio_num_t clk_pin,
               gpio_num_t dat3_pin, const char* mount_point, size_t max_files)
    : mosi_pin_(cmd_pin),    // CMD pin (was MOSI)
      miso_pin_(dat0_pin),   // DAT0 pin (was MISO)
      clk_pin_(clk_pin),     // CLK pin
      cs_pin_(dat3_pin),     // DAT3/CS pin (can be unused)
      mount_point_(mount_point),
      max_files_(max_files),
      mounted_(false),
      card_(nullptr),
      spi_host_(0) {  // Not used in SD mode
}

SDCard::~SDCard() {
    unmount();
}

bool SDCard::mount(bool format_if_failed) {
    if (mounted_) {
        ESP_LOGW(TAG, "SD card already mounted");
        return true;
    }

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using 1-bit SDIO mode");

    // Reset all SDMMC pins to ensure they're not held by other peripherals
    gpio_reset_pin(GPIO_NUM_14);  // CLK
    gpio_reset_pin(GPIO_NUM_15);  // CMD
    gpio_reset_pin(GPIO_NUM_2);   // DAT0
    gpio_reset_pin(GPIO_NUM_13);  // DAT3
    vTaskDelay(pdMS_TO_TICKS(10));

    // Configure all SDMMC pins with strong pull-ups for long wire (10cm CLK)
    gpio_set_pull_mode(GPIO_NUM_14, GPIO_PULLUP_ONLY);  // CLK
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);  // CMD
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);   // DAT0
    gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);  // DAT3

    // Pull DAT3 (GPIO13) HIGH to enable SD mode (not SPI mode)
    // DAT3 doubles as CS - must be high for SD card to respond to SD commands
    gpio_config_t dat3_conf = {};
    dat3_conf.intr_type = GPIO_INTR_DISABLE;
    dat3_conf.mode = GPIO_MODE_OUTPUT;
    dat3_conf.pin_bit_mask = (1ULL << GPIO_NUM_13);  // DAT3 is GPIO13
    dat3_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    dat3_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable pull-up
    gpio_config(&dat3_conf);
    gpio_set_level(GPIO_NUM_13, 1);  // Set HIGH for SD mode
    vTaskDelay(pdMS_TO_TICKS(10));   // Wait for card to recognize mode

    ESP_LOGI(TAG, "DAT3/CS pulled HIGH for SD mode");

    // Configure mount options
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = format_if_failed;
    mount_config.max_files = max_files_;
    mount_config.allocation_unit_size = 16 * 1024;

    // Use SDMMC host in 1-bit mode
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;  // Use 1-bit mode
    host.max_freq_khz = 400;  // Ultra-slow 400kHz for 10cm CLK wire signal integrity

    // Configure slot for 1-bit SD mode - MUST use hardware SDMMC pins
    // ESP32 SDMMC Slot 1 pins are FIXED and cannot be changed:
    // CLK=14, CMD=15, DAT0=2, DAT1=4, DAT2=12, DAT3=13
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;  // 1-bit mode
    slot_config.cd = SDMMC_SLOT_NO_CD;   // No card detect
    slot_config.wp = SDMMC_SLOT_NO_WP;   // No write protect
    // Note: pins are set automatically by SDMMC driver, cannot override

    ESP_LOGI(TAG, "Mounting filesystem");
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point_.c_str(), &host, 
                                            &slot_config, &mount_config, &card_);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s).", esp_err_to_name(ret));
        }
        return false;
    }

    mounted_ = true;
    ESP_LOGI(TAG, "Filesystem mounted");
    
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card_);

    return true;
}

void SDCard::unmount() {
    if (!mounted_) {
        return;
    }

    ESP_LOGI(TAG, "Unmounting SD card");
    
    esp_vfs_fat_sdcard_unmount(mount_point_.c_str(), card_);
    card_ = nullptr;
    mounted_ = false;
}

bool SDCard::isMounted() const {
    return mounted_;
}

bool SDCard::getCardInfo(std::string& name, std::string& type, uint32_t& speed, uint64_t& size_mb) {
    if (!mounted_ || card_ == nullptr) {
        return false;
    }

    name = card_->cid.name;
    
    // Determine card type based on capacity
    if (card_->is_sdio) {
        type = "SDIO";
    } else if (card_->is_mmc) {
        type = "MMC";
    } else {
        // SD card - check capacity
        uint64_t capacity = ((uint64_t)card_->csd.capacity) * card_->csd.sector_size;
        if (capacity > 2ULL * 1024 * 1024 * 1024) {
            type = "SDHC/SDXC";
        } else {
            type = "SDSC";
        }
    }

    speed = card_->max_freq_khz;
    size_mb = ((uint64_t)card_->csd.capacity) * card_->csd.sector_size / (1024 * 1024);

    return true;
}

uint64_t SDCard::getFreeSpace() {
    if (!mounted_) {
        return 0;
    }

    FATFS* fs;
    DWORD free_clusters;
    
    if (f_getfree("0:", &free_clusters, &fs) != FR_OK) {
        return 0;
    }

    uint64_t free_bytes = (uint64_t)free_clusters * fs->csize * fs->ssize;
    return free_bytes;
}

uint64_t SDCard::getTotalSpace() {
    if (!mounted_ || card_ == nullptr) {
        return 0;
    }

    return ((uint64_t)card_->csd.capacity) * card_->csd.sector_size;
}

const char* SDCard::getMountPoint() const {
    return mount_point_.c_str();
}

bool SDCard::writeFile(const char* filename, const void* data, size_t size, bool append) {
    if (!mounted_) {
        ESP_LOGE(TAG, "SD card not mounted");
        return false;
    }

    std::string fullpath = mount_point_ + "/" + filename;
    const char* mode = append ? "ab" : "wb";
    
    FILE* f = fopen(fullpath.c_str(), mode);
    if (f == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", fullpath.c_str());
        return false;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "Write failed: wrote %d bytes out of %d", written, size);
        return false;
    }

    ESP_LOGD(TAG, "Wrote %d bytes to %s", written, filename);
    return true;
}

size_t SDCard::readFile(const char* filename, void* buffer, size_t size) {
    if (!mounted_) {
        ESP_LOGE(TAG, "SD card not mounted");
        return 0;
    }

    std::string fullpath = mount_point_ + "/" + filename;
    
    FILE* f = fopen(fullpath.c_str(), "rb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", fullpath.c_str());
        return 0;
    }

    size_t read_bytes = fread(buffer, 1, size, f);
    fclose(f);

    ESP_LOGD(TAG, "Read %d bytes from %s", read_bytes, filename);
    return read_bytes;
}

bool SDCard::fileExists(const char* filename) {
    if (!mounted_) {
        return false;
    }

    std::string fullpath = mount_point_ + "/" + filename;
    struct stat st;
    return stat(fullpath.c_str(), &st) == 0;
}

bool SDCard::deleteFile(const char* filename) {
    if (!mounted_) {
        ESP_LOGE(TAG, "SD card not mounted");
        return false;
    }

    std::string fullpath = mount_point_ + "/" + filename;
    
    if (unlink(fullpath.c_str()) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s", fullpath.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Deleted file: %s", filename);
    return true;
}

} // namespace Bobot
