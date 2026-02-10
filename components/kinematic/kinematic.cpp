#include "kinematic.hpp"
#include <esp_log.h>
#include <cmath>

static const char* TAG = "Kinematic";

namespace Bobot {

Kinematic::Kinematic(ServoDriver* servo_driver)
    : servo(servo_driver), sequence_running(false), sequence_step(0), 
      step_timer(0), current_speed(300.0f) {
}

Kinematic::~Kinematic() {
}

bool Kinematic::startHousingSequence() {
    if (sequence_running) {
        ESP_LOGW(TAG, "Sequence already running");
        return false;
    }
    
    if (!servo) {
        ESP_LOGE(TAG, "Servo driver not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting housing sequence - all joints to 90° at 60 deg/s");
    
    // Set all 12 servos to 90 degrees at lowest speed (60 deg/s)
    for (int i = 0; i < 12; i++) {
        // Apply reverse channel mapping: UI channel N -> PCA9685 channel (15 - N)
        uint8_t pca_channel = 15 - i;
        servo->setAngleSmooth(pca_channel, 90, 60.0f);
    }
    
    return true;
}

bool Kinematic::startStandUpSequence(float speed_multiplier) {
    if (sequence_running) {
        ESP_LOGW(TAG, "Sequence already running");
        return false;
    }
    
    if (!servo) {
        ESP_LOGE(TAG, "Servo driver not initialized");
        return false;
    }
    
    // MG90S max speed: ~600 deg/s, half speed = 300 deg/s
    current_speed = 600.0f * speed_multiplier;
    
    if (current_speed < 60.0f) {
        current_speed = 60.0f;
    } else if (current_speed > 600.0f) {
        current_speed = 600.0f;
    }
    
    ESP_LOGI(TAG, "Starting stand-up sequence at %.0f deg/s", current_speed);
    
    sequence_running = true;
    sequence_step = 0;
    step_timer = 0;
    
    executeStandUpStep();
    
    return true;
}

void Kinematic::stopSequence() {
    if (sequence_running) {
        ESP_LOGI(TAG, "Stopping sequence at step %d", sequence_step);
        sequence_running = false;
        sequence_step = 0;
        step_timer = 0;
    }
}

void Kinematic::executeStandUpStep() {
    if (!sequence_running || !servo) {
        return;
    }
    
    ESP_LOGI(TAG, "Executing stand-up step %d", sequence_step);
    
    // Apply reverse channel mapping: UI channel N -> PCA9685 channel (15 - N)
    switch (sequence_step) {
        case 0:
            // Step 1: Set all j1 joints (UI ch 0, 3, 6, 9 -> PCA9685 ch 15, 12, 9, 6) to 90 degrees
            servo->setAngleSmooth(15, 90, current_speed);  // Back Left j1 (UI ch 0)
            servo->setAngleSmooth(12, 90, current_speed);  // Front Left j1 (UI ch 3)
            servo->setAngleSmooth(9, 90, current_speed);   // Front Right j1 (UI ch 6)
            servo->setAngleSmooth(6, 90, current_speed);   // Back Right j1 (UI ch 9)
            ESP_LOGI(TAG, "Step 1: Setting j1 joints to 90°");
            break;
            
        case 1:
            // Step 2: Set j2 joints (UI ch 1, 4, 7, 10 -> PCA9685 ch 14, 11, 8, 5) to 50 degrees
            servo->setAngleSmooth(14, 0, current_speed);  // Back Left j2 (UI ch 1)
            servo->setAngleSmooth(11, 0, current_speed);  // Front Left j2 (UI ch 4)
            servo->setAngleSmooth(8, 0, current_speed);   // Front Right j2 (UI ch 7)
            servo->setAngleSmooth(5, 0, current_speed);   // Back Right j2 (UI ch 10)
            ESP_LOGI(TAG, "Step 2: Setting j2 joints to 50°");
            break;
            
        case 2:
            // Step 3: Set j3 joints (UI ch 2, 5, 8, 11 -> PCA9685 ch 13, 10, 7, 4) to 140 degrees
            servo->setAngleSmooth(13, 180, current_speed);  // Back Left j3 (UI ch 2)
            servo->setAngleSmooth(10, 180, current_speed);  // Front Left j3 (UI ch 5)
            servo->setAngleSmooth(7, 180, current_speed);   // Front Right j3 (UI ch 8)
            servo->setAngleSmooth(4, 180, current_speed);   // Back Right j3 (UI ch 11)
            ESP_LOGI(TAG, "Step 3: Setting j3 joints to 140°");
            break;
            
        case 3:
            // Step 4: Set j2 joints to 140 degrees (stand up)
            servo->setAngleSmooth(14, 140, current_speed);  // Back Left j2 (UI ch 1)
            servo->setAngleSmooth(11, 140, current_speed);  // Front Left j2 (UI ch 4)
            servo->setAngleSmooth(8, 140, current_speed);   // Front Right j2 (UI ch 7)
            servo->setAngleSmooth(5, 140, current_speed);   // Back Right j2 (UI ch 10)
            ESP_LOGI(TAG, "Step 4: Standing up - j2 joints to 140°");
            break;
            
        case 4:
            // Step 5: Set j3 joints to 140 degrees (final position j1:90, j2:140, j3:140)
            servo->setAngleSmooth(13, 130, current_speed);  // Back Left j3 (UI ch 2)
            servo->setAngleSmooth(10, 130, current_speed);  // Front Left j3 (UI ch 5)
            servo->setAngleSmooth(7, 130, current_speed);   // Front Right j3 (UI ch 8)
            servo->setAngleSmooth(4, 130, current_speed);   // Back Right j3 (UI ch 11)
            ESP_LOGI(TAG, "Step 5: Final position - j3 joints to 140°");
            break;
            
        case 5:
            // Sequence complete
            ESP_LOGI(TAG, "Stand-up sequence complete!");
            sequence_running = false;
            sequence_step = 0;
            return;
    }
    
    sequence_step++;
}

void Kinematic::update(uint32_t delta_ms) {
    if (!sequence_running || !servo) {
        return;
    }
    
    step_timer += delta_ms;
    
    // Check if all servos have finished moving to current target
    if (!servo->isMoving()) {
        // Wait a bit between steps for stability
        if (step_timer >= 200) {  // 200ms pause between steps
            step_timer = 0;
            executeStandUpStep();
        }
    }
}

} // namespace Bobot
