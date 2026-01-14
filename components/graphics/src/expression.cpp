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
    : currentFrameIndex(0), loopType(LoopType::IdleBlink),
      animationFPS(20.0f), idleTimeMinMs(1000), idleTimeMaxMs(1000),
      animState(AnimationState::Idle), idleTimeRemainingMs(0),
      animTimeAccumulatorMs(0) {
}

Expression::~Expression() {
    frames.clear();
}

bool Expression::loadFromDirectory(const char* expressionPath) {
    ESP_LOGI(TAG, "Loading expression from: %s", expressionPath);

    // Clear existing data
    frames.clear();
    currentFrameIndex = 0;

    // Parse Description.ini
    char iniPath[256];
    snprintf(iniPath, sizeof(iniPath), "%s/Description.ini", expressionPath);
    if (!parseDescriptionIni(iniPath)) {
        ESP_LOGW(TAG, "Failed to parse Description.ini, using defaults");
        // Continue with defaults
    }

    // Load frames from Frames/ directory
    char framesDir[256];
    snprintf(framesDir, sizeof(framesDir), "%s/Frames", expressionPath);
    if (!loadFrames(framesDir)) {
        ESP_LOGE(TAG, "Failed to load frames from: %s", framesDir);
        return false;
    }

    // Initialize animation state
    if (loopType == LoopType::IdleBlink) {
        animState = AnimationState::Idle;
        idleTimeRemainingMs = generateIdleTime();
    } else {
        animState = AnimationState::Playing;
    }

    ESP_LOGI(TAG, "Expression loaded: %zu frames, FPS=%.1f, Type=%d", 
             frames.size(), animationFPS, static_cast<int>(loopType));
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

bool Expression::loadFrames(const char* framesDir) {
    // Count and load frames in order (Frame_00.bin, Frame_01.bin, ...)
    int frameIndex = 0;
    char framePath[256];

    while (true) {
        snprintf(framePath, sizeof(framePath), "%s/Frame_%02d.bin", 
                 framesDir, frameIndex);

        // Check if file exists
        FILE* testFile = fopen(framePath, "rb");
        if (!testFile) {
            break;  // No more frames
        }
        fclose(testFile);

        // Load frame
        auto frame = std::make_unique<Frame>();
        if (!frame->loadFromFile(framePath)) {
            ESP_LOGE(TAG, "Failed to load frame: %s", framePath);
            return false;
        }

        frames.push_back(std::move(frame));
        frameIndex++;
    }

    if (frames.empty()) {
        ESP_LOGE(TAG, "No frames found in: %s", framesDir);
        return false;
    }

    ESP_LOGI(TAG, "Loaded %zu frames", frames.size());
    return true;
}

void Expression::draw(u8g2_t* u8g2, const Vec2i& offset) {
    if (!isValid() || currentFrameIndex >= frames.size()) {
        return;
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
}

void Expression::update(uint32_t deltaTimeMs) {
    if (!isValid() || frames.size() <= 1) {
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
                             oldFrame, currentFrameIndex, frames.size());
                    
                    // Check if animation completed (reached end)
                    if (currentFrameIndex >= frames.size()) {
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
    if (currentFrameIndex >= frames.size()) {
        currentFrameIndex = 0;
    }
}

void Expression::setFrameIndex(size_t index) {
    if (!isValid()) return;
    
    if (index < frames.size()) {
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
    oss << "Expression(" << frames.size() << " frames, FPS=" << animationFPS;
    
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
