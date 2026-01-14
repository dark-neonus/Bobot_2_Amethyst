#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <u8g2.h>

#include "display.hpp"
#include "button_driver.hpp"
#include "sd_card.hpp"
#include "bmi160.hpp"
#include "asset_uploader.hpp"
#include "sdkconfig.h"

// Graphics engine
#include "vec2i.hpp"
#include "frame.hpp"
#include "expression.hpp"

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
static Bobot::AssetUploader* assetUploader = nullptr;
static i2c_master_bus_handle_t i2c_bus = nullptr;

// Upload mode flag
static volatile bool upload_mode_active = false;

// Recursive file counter
int countFilesRecursive(const char* path) {
  DIR* dir = opendir(path);
  if (!dir) {
    return 0;
  }
  
  int count = 0;
  struct dirent* entry;
  
  while ((entry = readdir(dir)) != nullptr) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
    
    if (entry->d_type == DT_DIR) {
      // Count directory itself
      count++;
      // Recursively count contents
      count += countFilesRecursive(full_path);
    } else {
      // Count file
      count++;
    }
  }
  
  closedir(dir);
  return count;
}

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
static int cached_file_count = 0;
static uint32_t last_count_time = 0;

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
  
  // Display total file count recursively (cached to avoid slowdown)
  if (sdCard != nullptr && cached_file_count > 0) {
    char file_info[32];
    snprintf(file_info, sizeof(file_info), "Total: %d files", cached_file_count);
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
  uint32_t button_hold_counter = 0;
  
  while (true) {
    // Check if upload mode was activated
    if (upload_mode_active) {
      ESP_LOGI(TAG, "Upload mode active, suspending UI task");
      
      // Display upload mode message
      display->clear();
      display->setFont(u8g2_font_6x10_tr);
      display->drawString(2, 20, "Upload Mode");
      display->drawString(2, 30, "Connect to:");
      display->drawString(2, 40, "Bobot_Upload");
      
      if (assetUploader && assetUploader->isActive()) {
        display->drawString(2, 50, "Ready!");
      }
      
      display->drawString(2, 60, "Hold Back to exit");
      
      display->update();
      
      // Wait for upload mode to finish (with timeout)
      uint32_t upload_timeout_counter = 0;
      const uint32_t UPLOAD_TIMEOUT_MS = 300000; // 5 minutes
      
      while (upload_mode_active && upload_timeout_counter < UPLOAD_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(500));
        upload_timeout_counter += 500;
        
        // Check for Back button to manually exit
        bool exit_button_states[9] = {false};
        if (buttonDriver->readButtons(exit_button_states) && exit_button_states[0]) {
          ESP_LOGI(TAG, "Manual exit via Back button");
          upload_mode_active = false;
          break;
        }
        
        // Check if uploader is no longer active (completed)
        if (assetUploader && !assetUploader->isActive()) {
          ESP_LOGI(TAG, "Upload completed, exiting upload mode");
          upload_mode_active = false;
          
          // Count and display total files
          int file_count = countFilesRecursive("/sdcard");
          ESP_LOGI(TAG, "Total files on SD card: %d", file_count);
          
          display->clear();
          display->setFont(u8g2_font_6x10_tr);
          display->drawString(2, 20, "Upload Complete!");
          char count_str[64];
          snprintf(count_str, sizeof(count_str), "Total: %d files", file_count);
          display->drawString(2, 35, count_str);
          display->drawString(2, 50, "Press Back to");
          display->drawString(2, 60, "continue");
          display->update();
          
          // Wait for Back button press
          bool confirm_button_states[9] = {false};
          while (true) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (buttonDriver->readButtons(confirm_button_states) && confirm_button_states[0]) {
              break;
            }
          }
          
          break;
        }
      }
      
      if (upload_timeout_counter >= UPLOAD_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Upload mode timed out after 5 minutes");
        upload_mode_active = false;
        if (assetUploader) {
          assetUploader->stop();
        }
      }
      
      ESP_LOGI(TAG, "Upload mode finished, resuming UI");
      continue;
    }
    
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
    
    // Update file count every 5 seconds
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (sdCard != nullptr && (current_time - last_count_time > 5000)) {
      cached_file_count = countFilesRecursive("/sdcard");
      last_count_time = current_time;
      needs_redraw = true;
    }
    
    // Check for button combination to activate upload mode
    // Hold Back + Settings + Debug buttons for 2 seconds
    bool combo_held = button_states[0] && button_states[6] && button_states[8];  // Back, Settings, Debug
    
    if (combo_held) {
      button_hold_counter++;
      ESP_LOGI(TAG, "Button combo held: %d/40", button_hold_counter);
      if (button_hold_counter >= 40) {  // 40 * 50ms = 2 seconds
        ESP_LOGI(TAG, "Upload mode button combination detected!");
        upload_mode_active = true;
        button_hold_counter = 0;
        
        // Start asset uploader
        if (assetUploader) {
          if (assetUploader->start()) {
            ESP_LOGI(TAG, "Asset uploader started successfully");
          } else {
            ESP_LOGE(TAG, "Failed to start asset uploader");
            upload_mode_active = false;
          }
        }
        continue;
      }
    } else {
      button_hold_counter = 0;
    }
    
    // Redraw UI if anything changed
    if (needs_redraw) {
      drawUI();
    }
    
    // Poll at 20 Hz (50ms)
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief UI mode switcher task - cycles between old UI and all expressions
 * 
 * Cycles through: Old UI -> Expression 1 -> Expression 2 -> ... -> Old UI
 * Switch triggered by holding UI button for 2 seconds
 */
void graphicsTestTask(void* parameter) {
  ESP_LOGI(TAG, "UI mode switcher task started");
  
  if (!display || !sdCard || !buttonDriver) {
    ESP_LOGE(TAG, "Display, SD card, or button driver not available");
    vTaskDelete(NULL);
    return;
  }
  
  // Scan Amethyst library for all expressions
  std::vector<std::string> expressionNames;
  char amethystPath[256];
  snprintf(amethystPath, sizeof(amethystPath), 
           "%s/assets/graphics/libraries/Amethyst",
           sdCard->getMountPoint());
  
  DIR* dir = opendir(amethystPath);
  if (dir != nullptr) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      // Skip . and ..
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }
      
      // Only add directories (expressions)
      if (entry->d_type == DT_DIR) {
        expressionNames.push_back(entry->d_name);
        ESP_LOGI(TAG, "Found expression: %s", entry->d_name);
      }
    }
    closedir(dir);
  }
  
  if (expressionNames.empty()) {
    ESP_LOGE(TAG, "No expressions found in Amethyst library");
    vTaskDelete(NULL);
    return;
  }
  
  ESP_LOGI(TAG, "Found %zu expressions in Amethyst library", expressionNames.size());
  
  // UI modes: -1 = old UI, 0+ = expression index
  int currentMode = -1;  // Start with old UI
  std::unique_ptr<Bobot::Graphics::Expression> currentExpression;
  bool expressionLoaded = false;
  
  uint32_t lastUpdateTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
  uint32_t uiButtonHoldTime = 0;
  bool uiButtonWasPressed = false;
  
  // Polling fallback for IMU
  uint32_t poll_counter = 0;
  uint32_t last_interrupt_count = 0;
  
  // Main loop
  while (true) {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    // Read IMU data continuously if available
    if (imu != nullptr) {
      bool should_read = false;
      
      // Check interrupt flag
      if (imu_data_ready) {
        imu_data_ready = false;
        should_read = true;
      }
      
      // Fallback: poll every 10 cycles (~500ms at 50Hz) if interrupts stop working
      poll_counter++;
      if (poll_counter >= 10) {
        poll_counter = 0;
        should_read = true;
        
        // Debug: check if interrupts are still firing
        if (imu_interrupt_count == last_interrupt_count) {
          ESP_LOGW(TAG, "No IMU interrupts - polling. Accel: %.2f,%.2f,%.2f",
                   accel_data.x, accel_data.y, accel_data.z);
        }
        last_interrupt_count = imu_interrupt_count;
      }
      
      if (should_read) {
        imu->readAccel(accel_data);
        imu->readGyro(gyro_data);
      }
    }
    // If IMU not available, accel_data stays at zeros (initialized at startup)
    
    // Check UI button state
    bool uiButtonPressed = buttonDriver->isButtonPressed(Bobot::Button::UI);
    
    // Detect UI button hold for 2 seconds
    if (uiButtonPressed && !uiButtonWasPressed) {
      // Button just pressed
      uiButtonHoldTime = 0;
      uiButtonWasPressed = true;
    } else if (uiButtonPressed && uiButtonWasPressed) {
      // Button being held
      uiButtonHoldTime += deltaTime;
      
      if (uiButtonHoldTime >= 2000) {
        // Held for 2 seconds - switch mode
        currentMode++;
        
        // Cycle: -1 (old UI) -> 0 -> 1 -> ... -> (n-1) -> -1
        if (currentMode >= (int)expressionNames.size()) {
          currentMode = -1;  // Back to old UI
        }
        
        ESP_LOGI(TAG, "Switching to mode %d", currentMode);
        
        // Load expression if needed
        if (currentMode >= 0) {
          char expressionPath[256];
          snprintf(expressionPath, sizeof(expressionPath), 
                   "%s/assets/graphics/libraries/Amethyst/%s",
                   sdCard->getMountPoint(),
                   expressionNames[currentMode].c_str());
          
          currentExpression = std::make_unique<Bobot::Graphics::Expression>();
          if (currentExpression->loadFromDirectory(expressionPath)) {
            expressionLoaded = true;
            ESP_LOGI(TAG, "Loaded expression: %s", expressionNames[currentMode].c_str());
          } else {
            ESP_LOGE(TAG, "Failed to load expression: %s", expressionNames[currentMode].c_str());
            expressionLoaded = false;
          }
        } else {
          expressionLoaded = false;
          currentExpression.reset();
        }
        
        // Wait for button release
        while (buttonDriver->isButtonPressed(Bobot::Button::UI)) {
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        uiButtonHoldTime = 0;
        uiButtonWasPressed = false;
      }
    } else if (!uiButtonPressed && uiButtonWasPressed) {
      // Button released before 2 seconds
      uiButtonHoldTime = 0;
      uiButtonWasPressed = false;
    }
    
    // Render current mode
    if (currentMode == -1) {
      // Show old UI
      // Read button states for UI
      buttonDriver->readButtons(button_states);
      
      drawUI();
      vTaskDelay(pdMS_TO_TICKS(50));  // 20 Hz for UI
    } else {
      // Show expression animation
      if (expressionLoaded && currentExpression) {
        currentExpression->update(deltaTime);
        
        display->clear();
        
        // Calculate offset based on accelerometer (robot tilt)
        // Accel X/Y show tilt direction - shift animation opposite to tilt
        // Limit shift to ±10 pixels, scale by ~2 pixels per m/s²
        int offsetX = (int)(-accel_data.y * 2.0f);
        int offsetY = (int)(accel_data.x * 2.0f);
        
        // Clamp to reasonable range
        if (offsetX > 10) offsetX = 10;
        if (offsetX < -10) offsetX = -10;
        if (offsetY > 10) offsetY = 10;
        if (offsetY < -10) offsetY = -10;
        
        // Log offset every 30 frames for debugging
        static int frame_count = 0;
        if (++frame_count >= 30) {
          frame_count = 0;
          ESP_LOGI(TAG, "Offset: (%d,%d) from Accel: (%.2f,%.2f,%.2f)",
                   offsetX, offsetY, accel_data.x, accel_data.y, accel_data.z);
        }
        
        Bobot::Graphics::Vec2i offset(offsetX, offsetY);
        currentExpression->draw(display->getU8g2Handle(), offset);
        
        // Display expression info at bottom
        display->setFont(u8g2_font_5x7_tr);
        char info[32];
        snprintf(info, sizeof(info), "%s F:%zu/%zu", 
                 expressionNames[currentMode].c_str(),
                 currentExpression->getFrameIndex() + 1,
                 currentExpression->getFrameCount());
        display->drawString(0, 63, info);
        
        display->update();
        vTaskDelay(pdMS_TO_TICKS(16));  // 60 FPS for animation
      } else {
        // Error loading expression
        display->clear();
        display->setFont(u8g2_font_6x10_tr);
        display->drawString(2, 20, "Error loading");
        display->drawString(2, 30, expressionNames[currentMode].c_str());
        display->update();
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
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
  
  // Scan I2C bus to see what devices are present
  ESP_LOGI(TAG, "Scanning I2C bus...");
  bool bmi160_found = false;
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = addr,
      .scl_speed_hz = 400000,
      .scl_wait_us = 0,
      .flags = {},
    };
    i2c_master_dev_handle_t temp_handle;
    if (i2c_master_bus_add_device(i2c_bus, &dev_cfg, &temp_handle) == ESP_OK) {
      // Try a quick probe (transmit 0 bytes)
      if (i2c_master_transmit(temp_handle, nullptr, 0, 100) == ESP_OK) {
        ESP_LOGI(TAG, "  Device at 0x%02X", addr);
        if (addr == 0x68 || addr == 0x69) {
          bmi160_found = true;
        }
      }
      i2c_master_bus_rm_device(temp_handle);
    }
  }
  
  if (!bmi160_found) {
    ESP_LOGW(TAG, "BMI160 not found on I2C bus - will use simulated data for testing");
  }
  
  // Initialize BMI160 IMU sensor on GPIO4 for INT1
  // Try both possible I2C addresses (0x68 if SDO=GND, 0x69 if SDO=VDD)
  ESP_LOGI(TAG, "Attempting BMI160 initialization...");
  imu = new Bobot::BMI160(i2c_bus, GPIO_NUM_4, 0x68);
  if (!imu->init()) {
    ESP_LOGW(TAG, "BMI160 init failed at 0x68, trying 0x69...");
    delete imu;
    imu = new Bobot::BMI160(i2c_bus, GPIO_NUM_4, 0x69);
    if (!imu->init()) {
      ESP_LOGE(TAG, "Failed to initialize BMI160 at both 0x68 and 0x69");
      // Continue without IMU
      delete imu;
      imu = nullptr;
    } else {
      ESP_LOGI(TAG, "BMI160 IMU initialized at 0x69");
    }
  } else {
    ESP_LOGI(TAG, "BMI160 IMU initialized at 0x68");
    
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
    
    // Initialize asset uploader
    assetUploader = new Bobot::AssetUploader(sdCard);
    ESP_LOGI(TAG, "Asset uploader initialized");
  } else {
    ESP_LOGW(TAG, "SD card not available - continuing without it");
    text_content = "No SD card";
  }
  
  // Draw initial UI
  drawUI();
  
  // Graphics test mode - comment out to use normal UI
  #define GRAPHICS_TEST_MODE 1
  
  #ifdef GRAPHICS_TEST_MODE
  // Create graphics test task
  xTaskCreate(graphicsTestTask, "graphics_test", 8192, NULL, 5, NULL);
  ESP_LOGI(TAG, "Graphics test mode enabled");
  #else
  // Create UI update task
  xTaskCreate(uiTask, "ui_task", 4096, NULL, 5, NULL);
  #endif
  
  ESP_LOGI(TAG, "Bobot initialized successfully!");
}
