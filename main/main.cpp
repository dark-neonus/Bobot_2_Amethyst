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
static i2c_master_bus_handle_t i2c_bus = nullptr;

// Button states
static bool button_states[9] = {false};

// SD card data
static std::string text_content = "";
static std::vector<std::string> debug_files;

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
  
  while (true) {
    // Read button states
    if (buttonDriver->readButtons(button_states)) {
      // Redraw UI with updated button states
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
