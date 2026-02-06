#include "frame.hpp"
#include "esp_log.h"
#include <cstdio>
#include <cstring>
#include <sstream>

static const char* TAG = "Frame";

namespace Bobot {
namespace Graphics {

Frame::Frame() : width(0), height(0), bitmapData(nullptr), bitmapSize(0) {
}

Frame::~Frame() {
    cleanup();
}

Frame::Frame(Frame&& other) noexcept
    : width(other.width), height(other.height), 
      bitmapData(other.bitmapData), bitmapSize(other.bitmapSize) {
    other.width = 0;
    other.height = 0;
    other.bitmapData = nullptr;
    other.bitmapSize = 0;
}

Frame& Frame::operator=(Frame&& other) noexcept {
    if (this != &other) {
        cleanup();
        width = other.width;
        height = other.height;
        bitmapData = other.bitmapData;
        bitmapSize = other.bitmapSize;
        
        other.width = 0;
        other.height = 0;
        other.bitmapData = nullptr;
        other.bitmapSize = 0;
    }
    return *this;
}

void Frame::cleanup() {
    if (bitmapData != nullptr) {
        delete[] bitmapData;
        bitmapData = nullptr;
    }
    width = 0;
    height = 0;
    bitmapSize = 0;
}

bool Frame::loadFromFile(const char* filePath, uint16_t frameWidth, uint16_t frameHeight) {
    cleanup();

    // Store provided dimensions
    width = frameWidth;
    height = frameHeight;

    // Open file
    FILE* file = fopen(filePath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open frame file: %s", filePath);
        return false;
    }

    ESP_LOGI(TAG, "Loading frame %s: %dx%d", filePath, width, height);

    // Calculate bitmap size
    // u8g2 format: 8 vertical pixels per byte, column-major order
    size_t bytesPerColumn = (height + 7) / 8;  // Round up to nearest byte
    bitmapSize = width * bytesPerColumn;

    // Allocate memory for bitmap data
    bitmapData = new (std::nothrow) uint8_t[bitmapSize];
    if (!bitmapData) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for bitmap", bitmapSize);
        fclose(file);
        cleanup();
        return false;
    }

    // Read bitmap data using DMA-enabled block read
    // NOTE: ESP-IDF's SDMMC peripheral uses DMA internally for data transfer
    // The fread() call triggers a VFS -> FatFS -> SDMMC driver chain where
    // the SDMMC hardware DMA controller handles the actual data movement.
    // 
    // OPTIMIZATION: By removing 4-byte header from each frame file:
    //   - Reduces per-frame overhead (4 bytes × N frames saved)
    //   - Single dimensions read from Description.ini vs N header reads
    //   - Faster file open/seek/read cycle (smaller files)
    //   - Better SD card block alignment (no odd 4-byte offset)
    // 
    // For future energy-efficient operation:
    //   - This read should be called immediately AFTER display update
    //   - Can be made async using FreeRTOS task + semaphore
    //   - CPU can enter light sleep while DMA transfers data
    //   - Current implementation: DMA active, but CPU waits (blocking)
    //   - Future implementation: DMA active, CPU sleeps (non-blocking)
    size_t bytesRead = fread(bitmapData, 1, bitmapSize, file);
    fclose(file);

    if (bytesRead != bitmapSize) {
        ESP_LOGE(TAG, "Failed to read bitmap data: expected %zu bytes, got %zu", 
                 bitmapSize, bytesRead);
        cleanup();
        return false;
    }

    ESP_LOGI(TAG, "Successfully loaded frame: %dx%d, %zu bytes", 
             width, height, bitmapSize);
    return true;
}

std::string Frame::toString() const {
    std::ostringstream oss;
    oss << "Frame(" << width << "x" << height << ", " << bitmapSize << " bytes)";
    return oss.str();
}

} // namespace Graphics
} // namespace Bobot
