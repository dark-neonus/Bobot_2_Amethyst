#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <stdio.h>
#include <string.h>
#include <u8g2.h>

#include "display.hpp"
#include "button_driver.hpp"
#include "sdkconfig.h"

// Forward declare C function from u8g2 HAL
extern "C" void u8g2_esp32_hal_set_i2c_bus(i2c_master_bus_handle_t bus_handle);

static const char* TAG = "Bobot";

// Global instances
static Bobot::Display* display = nullptr;
static Bobot::ButtonDriver* buttonDriver = nullptr;
static i2c_master_bus_handle_t i2c_bus = nullptr;

// Button states
static bool button_states[9] = {false};

/**
 * @brief Draw the UI with "meow" text and 9 button squares in 3x3 grid
 */
void drawUI() {
  display->clear();
  
  // Draw "meow" text at the top
  display->setFont(u8g2_font_lubI12_te);
  display->drawString(40, 15, "meow");
  
  // Draw 3x3 grid of button squares
  // Layout matches button arrangement:
  // [Back] [Up]   [UI]
  // [Left] [OK]   [Right]
  // [Set]  [Down] [Debug]
  
  const int square_size = 12;
  const int spacing = 4;
  const int start_x = 20;
  const int start_y = 25;
  
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
  
  // Draw initial UI
  drawUI();
  
  // Create UI update task
  xTaskCreate(uiTask, "ui_task", 4096, NULL, 5, NULL);
  
  ESP_LOGI(TAG, "Bobot initialized successfully!");
}
