#include "expression.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <algorithm>
#include <sstream>

static const char* TAG = "Expression";

namespace Bobot {
namespace Graphics {

Expression::Expression()
    : currentFrameIndex(0), totalFrameCount(0), expressionPath(""),
      loopType(LoopType::IdleBlink), animationFPS(20.0f), 
      idleTimeMinMs(1000), idleTimeMaxMs(1000),
      animState(AnimationState::Idle), idleTimeRemainingMs(0),
      animTimeAccumulatorMs(0), firstLoopComplete(false) {
}

Expression::~Expression() {
    frames.clear();
}

bool Expression::loadFromDirectory(const char* path) {
    ESP_LOGI(TAG, "Loading expression from: %s", path);

    // Store path for lazy loading
    this->expressionPath = path;

    // Clear existing data
    frames.clear();
    currentFrameIndex = 0;
    totalFrameCount = 0;

    // Parse Description.ini
    char iniPath[256];
    snprintf(iniPath, sizeof(iniPath), "%s/Description.ini", path);
    if (!parseDescriptionIni(iniPath)) {
        ESP_LOGW(TAG, "Failed to parse Description.ini, using defaults");
        // Continue with defaults
    }

    // STEP 1: Validate all frame files exist upfront
    char framesDir[256];
    snprintf(framesDir, sizeof(framesDir), "%s/Frames", path);
    totalFrameCount = validateFrames(framesDir);
    
    if (totalFrameCount == 0) {
        ESP_LOGE(TAG, "No valid frames found in: %s", framesDir);
        return false;
    }

    ESP_LOGI(TAG, "Validated %zu frames, setting up lazy loading", totalFrameCount);

    // STEP 2: Prepare frame vector with null pointers (lazy loading)
    frames.clear();
    frames.resize(totalFrameCount);

    // STEP 3: Load ONLY first frame for immediate display
    if (!loadFrame(0)) {
        ESP_LOGE(TAG, "Failed to load first frame");
        return false;
    }

    // Initialize animation state
    if (loopType == LoopType::IdleBlink) {
        animState = AnimationState::Idle;
        idleTimeRemainingMs = generateIdleTime();
    } else {
        animState = AnimationState::Playing;
    }

    ESP_LOGI(TAG, "Expression ready: %zu frames validated, FPS=%.1f, Type=%d", 
             totalFrameCount, animationFPS, static_cast<int>(loopType));
    ESP_LOGI(TAG, "First frame loaded immediately, remaining frames load on-demand");
    return true;
}

bool Expression::parseDescriptionIni(const char* iniPath) {
    FILE* file = fopen(iniPath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open Description.ini: %s", iniPath);
        return false;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        // Skip empty lines and comments
        if (len == 0 || line[0] == ';' || line[0] == '#' || line[0] == '[') {
            continue;
        }

        // Parse key = value pairs
        char* equals = strchr(line, '=');
        if (!equals) continue;

        // Extract key (trim whitespace)
        char key[128];
        size_t keyLen = equals - line;
        strncpy(key, line, keyLen);
        key[keyLen] = '\0';
        
        // Trim trailing whitespace from key
        while (keyLen > 0 && (key[keyLen-1] == ' ' || key[keyLen-1] == '\t')) {
            key[--keyLen] = '\0';
        }

        // Extract value (trim leading whitespace and comments)
        char* value = equals + 1;
        while (*value == ' ' || *value == '\t') value++;
        
        // Remove inline comments
        char* comment = strchr(value, ';');
        if (comment) *comment = '\0';
        
        // Trim trailing whitespace from value
        len = strlen(value);
        while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t')) {
            value[--len] = '\0';
        }

        // Parse known keys
        if (strcmp(key, "Type") == 0) {
            if (strcmp(value, "Loop") == 0) {
                loopType = LoopType::Loop;
            } else if (strcmp(value, "Image") == 0) {
                loopType = LoopType::Image;
            } else {
                loopType = LoopType::IdleBlink;
            }
            ESP_LOGI(TAG, "Loop type: %s", value);
        } else if (strcmp(key, "AnimationFPS") == 0) {
            animationFPS = atof(value);
            if (animationFPS <= 0) animationFPS = 20.0f;
            ESP_LOGI(TAG, "Animation FPS: %.1f", animationFPS);
        } else if (strcmp(key, "IdleTimeMinMS") == 0) {
            idleTimeMinMs = atoi(value);
            ESP_LOGI(TAG, "Idle time min: %u ms", idleTimeMinMs);
        } else if (strcmp(key, "IdleTimeMaxMS") == 0) {
            idleTimeMaxMs = atoi(value);
            ESP_LOGI(TAG, "Idle time max: %u ms", idleTimeMaxMs);
        }
    }

    fclose(file);
    return true;
}

size_t Expression::validateFrames(const char* framesDir) {
    // UPFRONT VALIDATION: Check all frame files exist before loading
    // This catches missing files early and calculates exact frame count
    
    size_t frameCount = 0;
    char framePath[256];

    ESP_LOGI(TAG, "Validating frames in: %s", framesDir);

    // Check frames sequentially: Frame_00.bin, Frame_01.bin, ...
    while (frameCount < 999) {  // Reasonable limit
        snprintf(framePath, sizeof(framePath), "%s/Frame_%02zu.bin", 
                 framesDir, frameCount);

        // Check if file exists and is readable
        FILE* testFile = fopen(framePath, "rb");
        if (!testFile) {
            // No more frames
            break;
        }

        // Verify file has minimum size (4 bytes header minimum)
        fseek(testFile, 0, SEEK_END);
        long fileSize = ftell(testFile);
        fclose(testFile);

        if (fileSize < 4) {
            ESP_LOGE(TAG, "Frame file too small: %s (%ld bytes)", framePath, fileSize);
            return 0;  // Validation failed
        }

        frameCount++;
    }

    if (frameCount == 0) {
        ESP_LOGE(TAG, "No frames found in: %s", framesDir);
    } else {
        ESP_LOGI(TAG, "Validated %zu frames successfully", frameCount);
    }

    return frameCount;
}

bool Expression::loadFrame(size_t frameIndex) {
    // LAZY LOADING: Load individual frame on-demand
    // Frames stay in memory once loaded (no unloading)
    
    if (frameIndex >= totalFrameCount) {
        ESP_LOGE(TAG, "Frame index %zu out of range (total: %zu)", 
                 frameIndex, totalFrameCount);
        return false;
    }

    // Already loaded?
    if (frames[frameIndex] != nullptr && frames[frameIndex]->isValid()) {
        return true;  // Frame already in memory
    }

    // Build frame path
    char framePath[256];
    snprintf(framePath, sizeof(framePath), "%s/Frames/Frame_%02zu.bin", 
             expressionPath.c_str(), frameIndex);

    // Load frame from SD card
    // NOTE: fread() internally uses DMA at SDMMC peripheral level
    // For future async operation, this call could be replaced with:
    //   - FatFS f_read() for more direct control
    //   - Async file reading task that loads during display update
    //   - DMA-direct block reading from SD card
    
    auto frame = std::make_unique<Frame>();
    if (!frame->loadFromFile(framePath)) {
        ESP_LOGE(TAG, "Failed to load frame %zu from: %s", frameIndex, framePath);
        return false;
    }

    frames[frameIndex] = std::move(frame);
    ESP_LOGD(TAG, "Lazily loaded frame %zu/%zu", frameIndex + 1, totalFrameCount);
    return true;
}

void Expression::preloadNextFrame() {
    // PRELOAD OPTIMIZATION: Load frames strategically after display update
    // 
    // IDLE STATE: Load ALL remaining frames during idle period before animation
    //             This ensures smooth playback at high FPS (40 FPS = 25ms/frame)
    //             Since loading takes 110ms but frame time is 25ms, we must
    //             preload everything before animation starts
    // 
    // PLAYING STATE: Load next frame only (for continuous loop animations)
    //
    // This positions SD card reads optimally:
    //   1. Display update completes
    //   2. This function called immediately after
    //   3. SD card DMA reads frames while CPU does other work
    //   4. Frames ready when animation starts
    
    if (!isValid()) return;

    switch (loopType) {
        case LoopType::Loop:
            if (!firstLoopComplete) {
                // FIRST LOOP: Load all remaining frames progressively
                // This spreads the load across first animation loop
                // Subsequent loops will be smooth (all frames cached)
                for (size_t i = 0; i < totalFrameCount; i++) {
                    if (frames[i] == nullptr || !frames[i]->isValid()) {
                        loadFrame(i);
                        return;  // Load one per draw() call
                    }
                }
                // All frames loaded!
                firstLoopComplete = true;
                ESP_LOGI(TAG, "All frames loaded - smooth playback ready!");
            } else {
                // SUBSEQUENT LOOPS: All frames cached, just verify next frame
                size_t nextIndex = (currentFrameIndex + 1) % totalFrameCount;
                if (frames[nextIndex] == nullptr || !frames[nextIndex]->isValid()) {
                    loadFrame(nextIndex);  // Shouldn't happen, but safe fallback
                }
            }
            break;
            
        case LoopType::IdleBlink:
            if (animState == AnimationState::Idle) {
                // IDLE: Load all remaining frames progressively
                // This spreads the load time across multiple draw() calls
                // during the idle period (1-5 seconds typically)
                for (size_t i = 0; i < totalFrameCount; i++) {
                    if (frames[i] == nullptr || !frames[i]->isValid()) {
                        loadFrame(i);
                        return;  // Load one per draw() call to avoid blocking
                    }
                }
                // All frames loaded during idle - animation will be smooth!
            } else {
                // PLAYING: Preload next frame for smooth playback
                size_t nextIndex = currentFrameIndex + 1;
                if (nextIndex >= totalFrameCount) {
                    nextIndex = 0;  // Will return to idle
                }
                if (frames[nextIndex] == nullptr || !frames[nextIndex]->isValid()) {
                    loadFrame(nextIndex);
                }
            }
            break;
            
        case LoopType::Image:
            return;  // Static image, no preload needed
    }
}

void Expression::draw(u8g2_t* u8g2, const Vec2i& offset) {
    if (!isValid() || currentFrameIndex >= totalFrameCount) {
        return;
    }

    // LAZY LOAD: Ensure current frame is loaded before drawing
    if (frames[currentFrameIndex] == nullptr || !frames[currentFrameIndex]->isValid()) {
        if (!loadFrame(currentFrameIndex)) {
            ESP_LOGE(TAG, "Failed to load frame %zu for drawing", currentFrameIndex);
            return;
        }
    }

    const Frame* frame = frames[currentFrameIndex].get();
    if (!frame || !frame->isValid()) {
        return;
    }

    // Custom draw function for vertical column-major bitmap format
    // Our frames are organized as: 8 vertical pixels per byte, column-major order
    // This means each byte represents a vertical column of 8 pixels
    const uint16_t width = frame->getWidth();
    const uint16_t height = frame->getHeight();
    const uint8_t* bitmap = frame->getBitmapData();
    
    const uint16_t bytes_per_column = (height + 7) / 8;  // Round up
    
    // Draw pixel by pixel from the column-major bitmap
    for (uint16_t x = 0; x < width; x++) {
        for (uint16_t y_byte = 0; y_byte < bytes_per_column; y_byte++) {
            uint8_t byte_val = bitmap[x * bytes_per_column + y_byte];
            
            // Each bit in the byte represents a pixel
            for (uint8_t bit = 0; bit < 8; bit++) {
                uint16_t y = y_byte * 8 + bit;
                if (y >= height) break;  // Don't draw beyond image height
                
                if (byte_val & (1 << bit)) {
                    // Pixel is on, draw it
                    u8g2_DrawPixel(u8g2, offset.x + x, offset.y + y);
                }
            }
        }
    }

    // OPTIMIZATION: Preload next frame after drawing current frame
    // This positions SD card reads right after display update for:
    //   - Better CPU utilization (DMA reads while CPU does other work)
    //   - Future async/sleep modes (load during display refresh)
    // CRITICAL: Called HERE (after draw) not in update() to ensure
    //           current frame is loaded before preloading next
    preloadNextFrame();
}

void Expression::update(uint32_t deltaTimeMs) {
    if (!isValid() || totalFrameCount <= 1) {
        return;
    }

    switch (loopType) {
        case LoopType::Image: {
            // Static image, no animation
            currentFrameIndex = 0;
            break;
        }

        case LoopType::Loop: {
            // Continuous loop animation
            animTimeAccumulatorMs += deltaTimeMs;
            
            // Calculate frame duration in milliseconds
            uint32_t frameDurationMs = static_cast<uint32_t>(1000.0f / animationFPS);
            
            while (animTimeAccumulatorMs >= frameDurationMs) {
                animTimeAccumulatorMs -= frameDurationMs;
                nextFrame();
            }
            break;
        }

        case LoopType::IdleBlink: {
            if (animState == AnimationState::Idle) {
                // Show first frame and wait
                currentFrameIndex = 0;
                
                if (idleTimeRemainingMs > deltaTimeMs) {
                    idleTimeRemainingMs -= deltaTimeMs;
                } else {
                    // Idle time finished, start animation
                    // We'll start from frame 1 on the NEXT frame advancement
                    idleTimeRemainingMs = 0;
                    animState = AnimationState::Playing;
                    currentFrameIndex = 0;  // Still on frame 0
                    animTimeAccumulatorMs = 0;
                    ESP_LOGI(TAG, "Starting blink animation from frame 0");
                }
            } else { // AnimationState::Playing
                // Play animation from frame 1 to last frame, then back to idle
                animTimeAccumulatorMs += deltaTimeMs;
                
                uint32_t frameDurationMs = static_cast<uint32_t>(1000.0f / animationFPS);
                
                while (animTimeAccumulatorMs >= frameDurationMs) {
                    animTimeAccumulatorMs -= frameDurationMs;
                    
                    // Advance to next frame
                    size_t oldFrame = currentFrameIndex;
                    currentFrameIndex++;
                    ESP_LOGI(TAG, "Frame advance: %zu -> %zu (total: %zu)", 
                             oldFrame, currentFrameIndex, totalFrameCount);
                    
                    // Check if animation completed (reached end)
                    if (currentFrameIndex >= totalFrameCount) {
                        currentFrameIndex = 0;  // Reset to idle frame
                        animState = AnimationState::Idle;
                        idleTimeRemainingMs = generateIdleTime();
                        ESP_LOGI(TAG, "Blink animation completed, back to idle (frame 0) for %u ms", 
                                 idleTimeRemainingMs);
                        break;
                    }
                }
            }
            break;
        }
    }
}

void Expression::nextFrame() {
    if (!isValid()) return;
    
    currentFrameIndex++;
    if (currentFrameIndex >= totalFrameCount) {
        currentFrameIndex = 0;
    }
}

void Expression::setFrameIndex(size_t index) {
    if (!isValid()) return;
    
    if (index < totalFrameCount) {
        currentFrameIndex = index;
    }
}

uint32_t Expression::generateIdleTime() {
    if (idleTimeMinMs >= idleTimeMaxMs) {
        return idleTimeMinMs;
    }
    
    uint32_t range = idleTimeMaxMs - idleTimeMinMs;
    uint32_t randomOffset = esp_random() % (range + 1);
    return idleTimeMinMs + randomOffset;
}

std::string Expression::toString() const {
    std::ostringstream oss;
    
    // Count actually loaded frames
    size_t loadedCount = 0;
    for (const auto& frame : frames) {
        if (frame != nullptr && frame->isValid()) {
            loadedCount++;
        }
    }
    
    oss << "Expression(" << loadedCount << "/" << totalFrameCount 
        << " frames loaded, FPS=" << animationFPS;
    
    switch (loopType) {
        case LoopType::IdleBlink:
            oss << ", IdleBlink";
            break;
        case LoopType::Loop:
            oss << ", Loop";
            break;
        case LoopType::Image:
            oss << ", Image";
            break;
    }
    
    oss << ")";
    return oss.str();
}

} // namespace Graphics
} // namespace Bobot
