#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <stdio.h>
#include <string.h>
#include <u8g2.h>

#include "display.hpp"
#include "button_driver.hpp"
#include "sd_card.hpp"
#include "bmi160.hpp"
#include "sdkconfig.h"

#include <dirent.h>
#include <vector>
#include <string>

// Forward declare C function from u8g2 HAL
extern "C" void u8g2_esp32_hal_set_i2c_bus(i2c_master_bus_handle_t bus_handle);

static const char* TAG = "Bobot";

// Global instances
static Bobot::Display* display = nullptr;
static Bobot::ButtonDriver* buttonDriver = nullptr;
static Bobot::SDCard* sdCard = nullptr;
static Bobot::BMI160* imu = nullptr;
static i2c_master_bus_handle_t i2c_bus = nullptr;

// Button states
static bool button_states[9] = {false};

// IMU data and interrupt flag
static volatile bool imu_data_ready = false;
static volatile uint32_t imu_interrupt_count = 0;
static Bobot::BMI160::AccelData accel_data = {0.0f, 0.0f, 0.0f};
static Bobot::BMI160::GyroData gyro_data = {0.0f, 0.0f, 0.0f};

// SD card data
static std::string text_content = "";
static std::vector<std::string> debug_files;

/**
 * @brief IMU interrupt handler (GPIO ISR)
 * 
 * Sets flag only - actual I2C read happens outside ISR
 * This is a non-interruptible interrupt handler
 */
static void IRAM_ATTR imu_isr_handler(void* arg) {
  // Just set the flag - no I2C operations in ISR
  imu_data_ready = true;
  uint32_t count = imu_interrupt_count;
  count++;
  imu_interrupt_count = count;
}

/**
 * @brief Draw the UI with "meow" text and 9 button squares in 3x3 grid
 */
void drawUI() {
  display->clear();
  
  // Draw "meow" text at the top
  display->setFont(u8g2_font_6x10_tr);
  display->drawString(2, 20, "meow");
  
  // Display SD card info
  if (!text_content.empty()) {
    display->drawString(2, 30, text_content.c_str());
  }
  
  // Display file count
  if (!debug_files.empty()) {
    char file_info[32];
    snprintf(file_info, sizeof(file_info), "Files: %d", debug_files.size());
    display->drawString(2, 40, file_info);
  }
  
  // Display IMU data on the left side
  display->setFont(u8g2_font_5x7_tr);
  char imu_buffer[32];
  
  // Accelerometer data (m/s²)
  snprintf(imu_buffer, sizeof(imu_buffer), "Ax:%.1f", accel_data.x);
  display->drawString(0, 50, imu_buffer);
  snprintf(imu_buffer, sizeof(imu_buffer), "Ay:%.1f", accel_data.y);
  display->drawString(0, 57, imu_buffer);
  snprintf(imu_buffer, sizeof(imu_buffer), "Az:%.1f", accel_data.z);
  display->drawString(0, 64, imu_buffer);
  
  // Gyroscope data (rad/s)
  snprintf(imu_buffer, sizeof(imu_buffer), "Gx:%.2f", gyro_data.x);
  display->drawString(64, 50, imu_buffer);
  snprintf(imu_buffer, sizeof(imu_buffer), "Gy:%.2f", gyro_data.y);
  display->drawString(64, 57, imu_buffer);
  snprintf(imu_buffer, sizeof(imu_buffer), "Gz:%.2f", gyro_data.z);
  display->drawString(64, 64, imu_buffer);
  
  // Draw 3x3 grid of button squares (smaller and lower)
  const int square_size = 4;
  const int spacing = 1;
  const int start_x = 0;
  const int start_y = 0;
  
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int button_idx = row * 3 + col;
      int x = start_x + col * (square_size + spacing);
      int y = start_y + row * (square_size + spacing);
      
      if (button_states[button_idx]) {
        // Button pressed - draw filled box
        display->drawBox(x, y, square_size, square_size);
      } else {
        // Button not pressed - draw frame
        display->drawFrame(x, y, square_size, square_size);
      }
    }
  }
  
  display->update();
}

/**
 * @brief Main UI task that polls buttons and updates display
 */
void uiTask(void* parameter) {
  ESP_LOGI(TAG, "UI task started");
  
  uint32_t poll_counter = 0;
  uint32_t last_interrupt_count = 0;
  
  while (true) {
    bool needs_redraw = false;
    
    // Check if IMU data is ready (interrupt fired OR polling fallback)
    if (imu != nullptr) {
      bool should_read = false;
      
      // Check interrupt flag
      if (imu_data_ready) {
        imu_data_ready = false;  // Clear flag
        should_read = true;
      }
      
      // Fallback: poll every 10 cycles (500ms) if interrupts stop working
      poll_counter++;
      if (poll_counter >= 10) {
        poll_counter = 0;
        should_read = true;
        
        // Debug: check if interrupts are still firing
        if (imu_interrupt_count == last_interrupt_count) {
          ESP_LOGW(TAG, "No IMU interrupts in 500ms - using polling fallback");
        }
        last_interrupt_count = imu_interrupt_count;
      }
      
      if (should_read) {
        // Read IMU data (outside ISR context)
        if (imu->readAccel(accel_data)) {
          needs_redraw = true;
        }
        if (imu->readGyro(gyro_data)) {
          needs_redraw = true;
        }
      }
    }
    
    // Read button states
    if (buttonDriver->readButtons(button_states)) {
      needs_redraw = true;
    }
    
    // Redraw UI if anything changed
    if (needs_redraw) {
      drawUI();
    }
    
    // Poll at 20 Hz (50ms)
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Bobot starting up...");
  
  // Initialize I2C master bus (shared by display and button driver)
  i2c_master_bus_config_t bus_config = {};
  bus_config.i2c_port = I2C_NUM_0;
  bus_config.sda_io_num = GPIO_NUM_21;
  bus_config.scl_io_num = GPIO_NUM_22;
  bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_config.glitch_ignore_cnt = 7;
  bus_config.flags.enable_internal_pullup = true;
  
  esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
    return;
  }
  
  ESP_LOGI(TAG, "I2C bus initialized");
  
  // Set I2C bus for u8g2 HAL to use shared bus
  u8g2_esp32_hal_set_i2c_bus(i2c_bus);
  
  // Initialize display
  display = new Bobot::Display(GPIO_NUM_21, GPIO_NUM_22, 0x78);
  if (!display->init()) {
    ESP_LOGE(TAG, "Failed to initialize display");
    return;
  }
  ESP_LOGI(TAG, "Display initialized");
  
  // Initialize button driver
  buttonDriver = new Bobot::ButtonDriver(i2c_bus, 0x20);
  if (!buttonDriver->init()) {
    ESP_LOGE(TAG, "Failed to initialize button driver");
    return;
  }
  ESP_LOGI(TAG, "Button driver initialized");
  
  // Initialize BMI160 IMU sensor on GPIO4 for INT1
  imu = new Bobot::BMI160(i2c_bus, GPIO_NUM_4, 0x68);
  if (!imu->init()) {
    ESP_LOGE(TAG, "Failed to initialize BMI160 IMU");
    // Continue without IMU
    delete imu;
    imu = nullptr;
  } else {
    ESP_LOGI(TAG, "BMI160 IMU initialized");
    
    // Configure GPIO interrupt for INT1 pin
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;  // Rising edge (active high)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_4);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Add ISR handler for INT1 pin
    gpio_isr_handler_add(GPIO_NUM_4, imu_isr_handler, nullptr);
    
    ESP_LOGI(TAG, "BMI160 interrupt configured on GPIO4");
    
    // Do initial sensor read to display current state (gravity)
    if (imu->readAccel(accel_data)) {
      ESP_LOGI(TAG, "Initial accel: X=%.2f Y=%.2f Z=%.2f m/s²", 
               accel_data.x, accel_data.y, accel_data.z);
    }
    if (imu->readGyro(gyro_data)) {
      ESP_LOGI(TAG, "Initial gyro: X=%.2f Y=%.2f Z=%.2f rad/s", 
               gyro_data.x, gyro_data.y, gyro_data.z);
    }
  }
  
  // Initialize SD card - Using 1-bit SDIO mode with FIXED hardware pins
  // ESP32 SDMMC Slot 1: CLK=GPIO14, CMD=GPIO15, DAT0=GPIO2
  // These pins CANNOT be changed - hardware limitation
  sdCard = new Bobot::SDCard();  // Pin parameters ignored, uses fixed pins
  if (sdCard->mount(false)) {
    ESP_LOGI(TAG, "SD card mounted successfully");
    
    // Read text from debug/text.txt
    char buffer[64];
    size_t read = sdCard->readFile("debug/text.txt", buffer, sizeof(buffer) - 1);
    if (read > 0) {
      buffer[read] = '\0';
      text_content = buffer;
      ESP_LOGI(TAG, "Read from SD: %s", text_content.c_str());
    } else {
      text_content = "No text.txt";
      ESP_LOGW(TAG, "Could not read debug/text.txt");
    }
    
    // List files in debug directory
    std::string debug_dir = std::string(sdCard->getMountPoint()) + "/debug";
    DIR* dir = opendir(debug_dir.c_str());
    if (dir != nullptr) {
      struct dirent* entry;
      while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
          debug_files.push_back(entry->d_name);
          ESP_LOGI(TAG, "Found file: %s", entry->d_name);
        }
      }
      closedir(dir);
    } else {
      ESP_LOGW(TAG, "Could not open debug directory");
      text_content = "No /debug";
    }
  } else {
    ESP_LOGW(TAG, "SD card not available - continuing without it");
    text_content = "No SD card";
  }
  
  // Draw initial UI
  drawUI();
  
  // Create UI update task
  xTaskCreate(uiTask, "ui_task", 4096, NULL, 5, NULL);
  
  ESP_LOGI(TAG, "Bobot initialized successfully!");
}
