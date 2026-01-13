#ifndef EXPRESSION_HPP
#define EXPRESSION_HPP

#include <vector>
#include <string>
#include <memory>
#include "frame.hpp"
#include "vec2i.hpp"
#include "u8g2.h"

namespace Bobot {
namespace Graphics {

/**
 * @brief Animation loop types
 */
enum class LoopType {
    IdleBlink,  // Display first frame, then play animation, repeat
    Loop,       // Continuously loop animation
    Image       // Display only first frame (static image)
};

/**
 * @brief Expression class for animated sequences
 * 
 * Holds a sequence of frames and manages animation playback
 */
class Expression {
public:
    Expression();
    ~Expression();

    /**
     * @brief Load expression from directory
     * @param expressionPath Path to expression directory (contains Description.ini and Frames/)
     * @return true if successful, false otherwise
     */
    bool loadFromDirectory(const char* expressionPath);

    /**
     * @brief Draw current frame on display at given offset
     * @param u8g2 Display handle
     * @param offset Position offset
     */
    void draw(u8g2_t* u8g2, const Vec2i& offset = Vec2i(0, 0));

    /**
     * @brief Update animation state (call this regularly based on FPS)
     * @param deltaTimeMs Time elapsed since last update in milliseconds
     */
    void update(uint32_t deltaTimeMs);

    /**
     * @brief Increment frame index
     */
    void nextFrame();

    /**
     * @brief Set current frame index
     * @param index Frame index (will be clamped to valid range)
     */
    void setFrameIndex(size_t index);

    /**
     * @brief Get current frame index
     */
    size_t getFrameIndex() const { return currentFrameIndex; }

    /**
     * @brief Get total number of frames
     */
    size_t getFrameCount() const { return frames.size(); }

    /**
     * @brief Get animation FPS
     */
    float getFPS() const { return animationFPS; }

    /**
     * @brief Get loop type
     */
    LoopType getLoopType() const { return loopType; }

    /**
     * @brief Check if expression is valid (has frames)
     */
    bool isValid() const { return !frames.empty(); }

    /**
     * @brief Get string representation
     */
    std::string toString() const;

private:
    std::vector<std::unique_ptr<Frame>> frames;
    size_t currentFrameIndex;
    LoopType loopType;
    float animationFPS;
    uint32_t idleTimeMinMs;
    uint32_t idleTimeMaxMs;

    // Animation state for IdleBlink mode
    enum class AnimationState {
        Idle,
        Playing
    };
    AnimationState animState;
    uint32_t idleTimeRemainingMs;
    uint32_t animTimeAccumulatorMs;

    /**
     * @brief Parse Description.ini file
     * @param iniPath Path to Description.ini
     * @return true if successful
     */
    bool parseDescriptionIni(const char* iniPath);

    /**
     * @brief Load all frames from Frames/ directory
     * @param framesDir Path to Frames directory
     * @return true if successful
     */
    bool loadFrames(const char* framesDir);

    /**
     * @brief Generate random idle time
     */
    uint32_t generateIdleTime();
};

} // namespace Graphics
} // namespace Bobot

#endif // EXPRESSION_HPP
