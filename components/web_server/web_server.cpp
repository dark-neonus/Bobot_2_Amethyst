#include "web_server.hpp"
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <string.h>

static const char* TAG = "WebServer";

namespace Bobot {

// Initialize static members
volatile bool WebServer::button_states[9] = {false};
portMUX_TYPE WebServer::button_mutex = portMUX_INITIALIZER_UNLOCKED;

// HTML page with 3x3 button matrix
static const char* HTML_PAGE = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Bobot Control</title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
            max-width: 500px;
            width: 100%;
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 10px;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .button-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 15px;
            margin-bottom: 20px;
        }
        .btn {
            aspect-ratio: 1;
            border: none;
            border-radius: 12px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.2s;
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.2);
            color: white;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 5px;
            user-select: none;
            -webkit-user-select: none;
            touch-action: manipulation;
        }
        .btn:active {
            transform: scale(0.95);
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
        }
        .btn.pressed {
            transform: scale(0.95);
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2) inset;
        }
        .btn-label {
            font-size: 12px;
            opacity: 0.9;
        }
        /* Button Colors */
        .btn-0 { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
        .btn-1 { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); }
        .btn-2 { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); }
        .btn-3 { background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%); }
        .btn-4 { background: linear-gradient(135deg, #fa709a 0%, #fee140 100%); }
        .btn-5 { background: linear-gradient(135deg, #30cfd0 0%, #330867 100%); }
        .btn-6 { background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%); }
        .btn-7 { background: linear-gradient(135deg, #ff9a56 0%, #ff6a88 100%); }
        .btn-8 { background: linear-gradient(135deg, #ffecd2 0%, #fcb69f 100%); }
        
        .status {
            text-align: center;
            padding: 10px;
            margin-top: 20px;
            border-radius: 8px;
            background: #f0f0f0;
            font-size: 14px;
            color: #333;
        }
        .status.connected {
            background: #d4edda;
            color: #155724;
        }
        .status.error {
            background: #f8d7da;
            color: #721c24;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 Bobot Control</h1>
        <div class="subtitle">Touch and hold buttons to control</div>
        <div class="button-grid">
            <button class="btn btn-0" data-btn="0">
                <span>⬅️</span>
                <span class="btn-label">BACK</span>
            </button>
            <button class="btn btn-1" data-btn="1">
                <span>⬆️</span>
                <span class="btn-label">UP</span>
            </button>
            <button class="btn btn-2" data-btn="2">
                <span>📱</span>
                <span class="btn-label">UI</span>
            </button>
            <button class="btn btn-3" data-btn="3">
                <span>◀️</span>
                <span class="btn-label">LEFT</span>
            </button>
            <button class="btn btn-4" data-btn="4">
                <span>⭕</span>
                <span class="btn-label">OK</span>
            </button>
            <button class="btn btn-5" data-btn="5">
                <span>▶️</span>
                <span class="btn-label">RIGHT</span>
            </button>
            <button class="btn btn-6" data-btn="6">
                <span>⚙️</span>
                <span class="btn-label">SETTINGS</span>
            </button>
            <button class="btn btn-7" data-btn="7">
                <span>⬇️</span>
                <span class="btn-label">DOWN</span>
            </button>
            <button class="btn btn-8" data-btn="8">
                <span>🐛</span>
                <span class="btn-label">DEBUG</span>
            </button>
        </div>
        <div class="status" id="status">Connecting...</div>
    </div>

    <script>
        const buttons = document.querySelectorAll('.btn');
        const statusDiv = document.getElementById('status');
        let activeButtons = new Set();

        function updateStatus(message, type = '') {
            statusDiv.textContent = message;
            statusDiv.className = 'status ' + type;
        }

        function sendButtonState(btnId, pressed) {
            fetch('/api/button', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ button: btnId, pressed: pressed })
            })
            .then(response => {
                if (!response.ok) throw new Error('Failed to send');
                return response.json();
            })
            .then(data => {
                if (data.success) {
                    updateStatus(`Connected | Button ${btnId}: ${pressed ? 'ON' : 'OFF'}`, 'connected');
                }
            })
            .catch(err => {
                updateStatus('Connection error!', 'error');
                console.error('Error:', err);
            });
        }

        buttons.forEach(btn => {
            const btnId = parseInt(btn.dataset.btn);
            
            // Mouse events
            btn.addEventListener('mousedown', (e) => {
                e.preventDefault();
                if (!activeButtons.has(btnId)) {
                    activeButtons.add(btnId);
                    btn.classList.add('pressed');
                    sendButtonState(btnId, true);
                }
            });
            
            btn.addEventListener('mouseup', (e) => {
                e.preventDefault();
                if (activeButtons.has(btnId)) {
                    activeButtons.delete(btnId);
                    btn.classList.remove('pressed');
                    sendButtonState(btnId, false);
                }
            });
            
            btn.addEventListener('mouseleave', (e) => {
                if (activeButtons.has(btnId)) {
                    activeButtons.delete(btnId);
                    btn.classList.remove('pressed');
                    sendButtonState(btnId, false);
                }
            });
            
            // Touch events
            btn.addEventListener('touchstart', (e) => {
                e.preventDefault();
                if (!activeButtons.has(btnId)) {
                    activeButtons.add(btnId);
                    btn.classList.add('pressed');
                    sendButtonState(btnId, true);
                }
            });
            
            btn.addEventListener('touchend', (e) => {
                e.preventDefault();
                if (activeButtons.has(btnId)) {
                    activeButtons.delete(btnId);
                    btn.classList.remove('pressed');
                    sendButtonState(btnId, false);
                }
            });
            
            btn.addEventListener('touchcancel', (e) => {
                if (activeButtons.has(btnId)) {
                    activeButtons.delete(btnId);
                    btn.classList.remove('pressed');
                    sendButtonState(btnId, false);
                }
            });
        });

        // Prevent context menu on long press
        document.addEventListener('contextmenu', e => e.preventDefault());

        // Initial connection test
        setTimeout(() => {
            fetch('/api/state')
                .then(response => response.json())
                .then(data => {
                    updateStatus('Connected to Bobot!', 'connected');
                })
                .catch(err => {
                    updateStatus('Please check connection', 'error');
                });
        }, 500);
    </script>
</body>
</html>
)HTML";

WebServer::WebServer() : server(nullptr), server_task(nullptr) {
    // Initialize button states to false
    for (int i = 0; i < 9; i++) {
        button_states[i] = false;
    }
}

WebServer::~WebServer() {
    stop();
}

esp_err_t WebServer::init(const Config& cfg) {
    config = cfg;
    
    ESP_LOGI(TAG, "Initializing Web Server on Core %d", config.core_id);
    
    // Initialize NVS (required for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Initialize Wi-Fi in AP mode
    ret = initWiFi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi");
        return ret;
    }
    
    // Start HTTP server
    ret = startHTTPServer();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ret;
    }
    
    ESP_LOGI(TAG, "Web Server started successfully");
    ESP_LOGI(TAG, "Connect to Wi-Fi: %s", config.ssid);
    ESP_LOGI(TAG, "Open browser to: http://%s", getIPAddress());
    
    return ESP_OK;
}

esp_err_t WebServer::stop() {
    if (server) {
        httpd_stop(server);
        server = nullptr;
    }
    
    // Stop Wi-Fi
    esp_wifi_stop();
    esp_wifi_deinit();
    
    return ESP_OK;
}

void WebServer::readButtons(bool* out_button_states) {
    portENTER_CRITICAL(&button_mutex);
    memcpy(out_button_states, (void*)button_states, sizeof(bool) * 9);
    portEXIT_CRITICAL(&button_mutex);
}

bool WebServer::isButtonPressed(uint8_t button_idx) {
    if (button_idx >= 9) return false;
    
    bool pressed;
    portENTER_CRITICAL(&button_mutex);
    pressed = button_states[button_idx];
    portEXIT_CRITICAL(&button_mutex);
    
    return pressed;
}

esp_err_t WebServer::initWiFi() {
    // Create AP network interface
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    
    // Configure Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {};
    
    // Set SSID
    strncpy((char*)wifi_config.ap.ssid, config.ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(config.ssid);
    
    // Set password (empty = open network)
    if (config.password && strlen(config.password) > 0) {
        strncpy((char*)wifi_config.ap.password, config.password, sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    wifi_config.ap.max_connection = config.max_connections;
    wifi_config.ap.channel = 1;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi AP started: SSID=%s, Channel=1", config.ssid);
    
    return ESP_OK;
}

esp_err_t WebServer::startHTTPServer() {
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = config.port;
    server_config.max_uri_handlers = 8;
    server_config.core_id = config.core_id;  // Run on specified core
    server_config.task_priority = 5;
    server_config.stack_size = 8192;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d, core %d", config.port, config.core_id);
    
    if (httpd_start(&server, &server_config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = rootHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t button_uri = {
            .uri = "/api/button",
            .method = HTTP_POST,
            .handler = buttonHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &button_uri);
        
        httpd_uri_t state_uri = {
            .uri = "/api/state",
            .method = HTTP_GET,
            .handler = stateHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &state_uri);
        
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
}

esp_err_t WebServer::rootHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::buttonHandler(httpd_req_t* req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse JSON: {"button": N, "pressed": true/false}
    int button_id = -1;
    bool pressed = false;
    
    // Simple JSON parsing
    char* btn_ptr = strstr(buf, "\"button\"");
    char* pressed_ptr = strstr(buf, "\"pressed\"");
    
    if (btn_ptr && pressed_ptr) {
        char* colon = strchr(btn_ptr, ':');
        if (colon) {
            button_id = atoi(colon + 1);
        }
        
        pressed = (strstr(pressed_ptr, "true") != nullptr);
    }
    
    // Validate button ID
    if (button_id >= 0 && button_id < 9) {
        portENTER_CRITICAL(&button_mutex);
        button_states[button_id] = pressed;
        portEXIT_CRITICAL(&button_mutex);
        
        ESP_LOGI(TAG, "Button %d: %s", button_id, pressed ? "PRESSED" : "RELEASED");
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": true}");
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Invalid button ID\"}");
    }
    
    return ESP_OK;
}

esp_err_t WebServer::stateHandler(httpd_req_t* req) {
    char response[256];
    
    portENTER_CRITICAL(&button_mutex);
    snprintf(response, sizeof(response),
             "{\"buttons\": [%d,%d,%d,%d,%d,%d,%d,%d,%d]}",
             button_states[0], button_states[1], button_states[2],
             button_states[3], button_states[4], button_states[5],
             button_states[6], button_states[7], button_states[8]);
    portEXIT_CRITICAL(&button_mutex);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    
    return ESP_OK;
}

} // namespace Bobot
