#ifndef FRAME_HPP
#define FRAME_HPP

#include <cstdint>
#include <string>
#include "vec2i.hpp"

namespace Bobot {
namespace Graphics {

/**
 * @brief Frame class holding bitmap data in u8g2-compatible format
 * 
 * Binary format:
 * - Bytes 0-1: width (little-endian uint16)
 * - Bytes 2-3: height (little-endian uint16)
 * - Remaining bytes: monochrome bitmap data (8 vertical pixels per byte, column-major)
 */
class Frame {
public:
    Frame();
    ~Frame();

    // Delete copy constructor and assignment operator to prevent double-free
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    // Move constructor and assignment operator
    Frame(Frame&& other) noexcept;
    Frame& operator=(Frame&& other) noexcept;

    /**
     * @brief Load frame from file path using DMA
     * @param filePath Path to the frame binary file
     * @return true if successful, false otherwise
     */
    bool loadFromFile(const char* filePath);

    /**
     * @brief Get frame width
     */
    uint16_t getWidth() const { return width; }

    /**
     * @brief Get frame height
     */
    uint16_t getHeight() const { return height; }

    /**
     * @brief Get bitmap data pointer (for u8g2 drawing)
     */
    const uint8_t* getBitmapData() const { return bitmapData; }

    /**
     * @brief Check if frame is valid (has data)
     */
    bool isValid() const { return bitmapData != nullptr; }

    /**
     * @brief Get string representation
     */
    std::string toString() const;

private:
    uint16_t width;
    uint16_t height;
    uint8_t* bitmapData;  // Dynamically allocated bitmap data
    size_t bitmapSize;    // Size of bitmap data in bytes

    void cleanup();
};

} // namespace Graphics
} // namespace Bobot

#endif // FRAME_HPP
