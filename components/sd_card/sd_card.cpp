#include "sd_card.hpp"
#include <esp_log.h>
#include <driver/spi_common.h>
#include <driver/gpio.h>
#include <sys/stat.h>
#include <sys/unistd.h>

extern "C" {
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
}

static const char* TAG = "SDCard";

namespace Bobot {

SDCard::SDCard(gpio_num_t mosi_pin, gpio_num_t miso_pin, gpio_num_t clk_pin,
               gpio_num_t cs_pin, const char* mount_point, size_t max_files)
    : mosi_pin_(mosi_pin),
      miso_pin_(miso_pin),
      clk_pin_(clk_pin),
      cs_pin_(cs_pin),
      mount_point_(mount_point),
      max_files_(max_files),
      mounted_(false),
      card_(nullptr),
      spi_host_(SPI3_HOST) {  // VSPI for pins 23,19,18
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
    ESP_LOGI(TAG, "Using SPI peripheral");

    // Reset SD card by toggling CS pin (forces card to idle state)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << cs_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Pulse CS low to reset card state
    gpio_set_level(cs_pin_, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(cs_pin_, 1);
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for card to stabilize
    
    // Now release CS pin for SPI driver to take over
    gpio_reset_pin(cs_pin_);
    
    ESP_LOGI(TAG, "SD card reset sequence complete");

    // Configure mount options - exactly like official example
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = format_if_failed;
    mount_config.max_files = max_files_;
    mount_config.allocation_unit_size = 16 * 1024;

    // Use SDSPI host with very slow clock speed for difficult cards/breadboard
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 100;  // Ultra-slow 100kHz for maximum compatibility

    // Configure SPI bus - exactly like official example
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = mosi_pin_;
    bus_cfg.miso_io_num = miso_pin_;
    bus_cfg.sclk_io_num = clk_pin_;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return false;
    }

    // Configure device slot - exactly like official example
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs_pin_;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point_.c_str(), &host, &slot_config, 
                                   &mount_config, &card_);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        spi_bus_free((spi_host_device_t)host.slot);
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
    
    // Get the host slot before unmounting
    spi_host_device_t host_slot = (card_ && card_->host.slot != -1) 
                                    ? (spi_host_device_t)card_->host.slot 
                                    : SPI3_HOST;
    
    esp_vfs_fat_sdcard_unmount(mount_point_.c_str(), card_);
    spi_bus_free(host_slot);
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
