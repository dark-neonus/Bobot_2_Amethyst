#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include <esp_http_server.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>

// Forward declarations
namespace Bobot {
    class ServoDriver;
    class Kinematic;
}

namespace Bobot {

/**
 * @brief Web Server for Robot Control
 * 
 * Provides a web interface with 3x3 button matrix to control the robot.
 * Runs on Core 1 (separate from SD card and main control loop on Core 0).
 */
class WebServer {
public:
    /**
     * @brief Configuration structure for Web Server
     */
    struct Config {
        const char* ssid;           ///< Wi-Fi AP SSID
        const char* password;       ///< Wi-Fi AP password (min 8 chars, use "" for open)
        uint16_t port;              ///< HTTP server port (default: 80)
        uint8_t max_connections;    ///< Maximum simultaneous connections
        int core_id;                ///< Core to run server task on (0 or 1)
    };

    /**
     * @brief Default configuration
     */
    static constexpr Config DefaultConfig = {
        .ssid = "Bobot_Control",
        .password = "",  // Open network
        .port = 80,
        .max_connections = 4,
        .core_id = 1  // Run on Core 1
    };

    /**
     * @brief Construct a new Web Server object
     */
    WebServer();

    /**
     * @brief Destroy the Web Server object
     */
    ~WebServer();

    /**
     * @brief Initialize and start the web server
     * 
     * @param config Configuration structure
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t init(const Config& config = DefaultConfig);

    /**
     * @brief Stop the web server and Wi-Fi
     * 
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t stop();

    /**
     * @brief Read the current state of all virtual buttons
     * 
     * @param button_states Output array of 9 button states (true = pressed)
     */
    void readButtons(bool* button_states);

    /**
     * @brief Check if a specific button is pressed
     * 
     * @param button_idx Button index (0-8)
     * @return true if button is currently pressed
     */
    bool isButtonPressed(uint8_t button_idx);

    /**
     * @brief Get the IP address of the access point
     * 
     * @return const char* IP address string
     */
    const char* getIPAddress() const { return "192.168.4.1"; }

    /**
     * @brief Set servo driver for servo control
     * 
     * @param driver Pointer to servo driver
     */
    void setServoDriver(ServoDriver* driver);

    /**
     * @brief Set kinematic controller for movement sequences
     * 
     * @param kin Pointer to kinematic controller
     */
    void setKinematic(Kinematic* kin);

private:
    httpd_handle_t server;
    Config config;
    TaskHandle_t server_task;
    
    // Servo and kinematic control
    static ServoDriver* servo_driver;
    static Kinematic* kinematic;
    static float servo_speed;  // degrees per second
    
    // Button states (shared between web handlers and main code)
    static volatile bool button_states[9];
    static portMUX_TYPE button_mutex;

    /**
     * @brief Initialize Wi-Fi in AP mode
     */
    esp_err_t initWiFi();

    /**
     * @brief Start HTTP server
     */
    esp_err_t startHTTPServer();

    /**
     * @brief HTTP handler for root page (/)
     */
    static esp_err_t rootHandler(httpd_req_t* req);

    /**
     * @brief HTTP handler for button press API (/api/button)
     */
    static esp_err_t buttonHandler(httpd_req_t* req);

    /**
     * @brief HTTP handler for servo angle setting (/api/servo)
     */
    static esp_err_t servoHandler(httpd_req_t* req);

    /**
     * @brief HTTP handler for servo state query (/api/servo/state)
     */
    static esp_err_t servoStateHandler(httpd_req_t* req);

    /**
     * @brief HTTP handler for speed setting (/api/speed)
     */
    static esp_err_t speedHandler(httpd_req_t* req);

    /**
     * @brief HTTP handler for kinematic commands (/api/kinematic)
     */
    static esp_err_t kinematicHandler(httpd_req_t* req);

    /**
     * @brief HTTP handler for button state query (/api/state)
     */
    static esp_err_t stateHandler(httpd_req_t* req);
};

} // namespace Bobot

#endif // WEB_SERVER_HPP
