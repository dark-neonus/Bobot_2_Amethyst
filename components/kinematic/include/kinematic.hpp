#ifndef KINEMATIC_HPP
#define KINEMATIC_HPP

#include "servo_driver.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>

namespace Bobot {

/**
 * @brief Kinematic controller for robot movement sequences
 * 
 * Provides high-level movement functions like stand-up sequence
 */
class Kinematic {
public:
    /**
     * @brief Construct a new Kinematic controller
     * 
     * @param servo_driver Pointer to initialized servo driver
     */
    Kinematic(ServoDriver* servo_driver);

    /**
     * @brief Destroy the Kinematic controller
     */
    ~Kinematic();

    /**
     * @brief Start the stand-up sequence
     * 
     * Performs coordinated movement to stand robot up from lying position
     * @param speed_multiplier Speed multiplier (0.5 = half speed, 1.0 = full speed)
     * @return true if sequence started successfully
     */
    bool startStandUpSequence(float speed_multiplier = 0.5f);

    /**
     * @brief Start housing sequence (move all joints to 90 degrees slowly)
     * 
     * @return true if sequence started successfully
     */
    bool startHousingSequence();

    /**
     * @brief Stop any running sequence
     */
    void stopSequence();

    /**
     * @brief Check if sequence is running
     * 
     * @return true if sequence is active
     */
    bool isSequenceRunning() const { return sequence_running; }

    /**
     * @brief Update kinematic controller (call periodically)
     * 
     * @param delta_ms Time since last update in milliseconds
     */
    void update(uint32_t delta_ms);

private:
    ServoDriver* servo;
    bool sequence_running;
    uint8_t sequence_step;
    uint32_t step_timer;
    float current_speed;
    
    // Leg joint indices
    // Back Left: 0-2, Front Left: 3-5, Front Right: 6-8, Back Right: 9-11
    // i+0: horizontal rotation, i+1: first vertical (strong), i+2: second vertical (weak)
    
    /**
     * @brief Execute next step of stand-up sequence
     */
    void executeStandUpStep();
};

} // namespace Bobot

#endif // KINEMATIC_HPP
