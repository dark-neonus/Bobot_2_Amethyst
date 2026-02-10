#include "web_server.hpp"
#include "servo_driver.hpp"
#include "kinematic.hpp"
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <string.h>
#include <stdio.h>

static const char* TAG = "WebServer";

namespace Bobot {

// Initialize static members
volatile bool WebServer::button_states[9] = {false};
portMUX_TYPE WebServer::button_mutex = portMUX_INITIALIZER_UNLOCKED;
ServoDriver* WebServer::servo_driver = nullptr;
Kinematic* WebServer::kinematic = nullptr;
float WebServer::servo_speed = 300.0f;  // Default speed

// HTML page with controls and servo interface
static const char* HTML_PAGE_PART1 = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Bobot Control</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 15px;
            min-height: 100vh;
        }
        .container { max-width: 800px; margin: 0 auto; }
        .panel {
            background: white;
            border-radius: 15px;
            padding: 20px;
            margin-bottom: 15px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        }
        h1 { text-align: center; color: #333; margin-bottom: 5px; font-size: 24px; }
        .subtitle { text-align: center; color: #666; margin-bottom: 15px; font-size: 12px; }
        h2 { color: #555; font-size: 18px; margin-bottom: 10px; border-bottom: 2px solid #667eea; padding-bottom: 5px; }
        
        /* Button Grid */
        .button-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin-bottom: 10px;
        }
        .btn {
            aspect-ratio: 1;
            border: none;
            border-radius: 10px;
            font-size: 14px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.2s;
            box-shadow: 0 3px 8px rgba(0,0,0,0.2);
            color: white;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 3px;
            user-select: none;
            touch-action: manipulation;
        }
        .btn:active, .btn.pressed { transform: scale(0.95); box-shadow: 0 2px 4px rgba(0,0,0,0.2) inset; }
        .btn-label { font-size: 10px; opacity: 0.9; }
        .btn-0 { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
        .btn-1 { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); }
        .btn-2 { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); }
        .btn-3 { background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%); }
        .btn-4 { background: linear-gradient(135deg, #fa709a 0%, #fee140 100%); }
        .btn-5 { background: linear-gradient(135deg, #30cfd0 0%, #330867 100%); }
        .btn-6 { background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%); }
        .btn-7 { background: linear-gradient(135deg, #ff9a56 0%, #ff6a88 100%); }
        .btn-8 { background: linear-gradient(135deg, #ffecd2 0%, #fcb69f 100%); }
        
        /* Servo Controls */
        .servo-section { margin-bottom: 15px; }
        .leg-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
            margin-bottom: 15px;
        }
        .leg-card {
            background: #f8f9fa;
            padding: 12px;
            border-radius: 10px;
            border: 2px solid #667eea;
        }
        .leg-title {
            font-weight: bold;
            color: #667eea;
            margin-bottom: 8px;
            font-size: 14px;
        }
        .servo-control {
            margin-bottom: 8px;
        }
        .servo-label {
            font-size: 11px;
            color: #666;
            margin-bottom: 3px;
            display: flex;
            justify-content: space-between;
        }
        .servo-value {
            font-weight: bold;
            color: #333;
        }
        input[type="range"] {
            width: 100%;
            height: 6px;
            border-radius: 3px;
            background: #ddd;
            outline: none;
            margin: 3px 0;
        }
        input[type="range"]::-webkit-slider-thumb {
            appearance: none;
            width: 18px;
            height: 18px;
            border-radius: 50%;
            background: #667eea;
            cursor: pointer;
        }
        
        /* Speed Control */
        .speed-control {
            background: #fff3cd;
            padding: 12px;
            border-radius: 10px;
            margin-bottom: 15px;
            border: 2px solid #ffc107;
        }
        .speed-label {
            font-weight: bold;
            color: #856404;
            font-size: 13px;
            margin-bottom: 5px;
            display: flex;
            justify-content: space-between;
        }
        
        /* Kinematic Controls */
        .kinematic-controls {
            display: flex;
            gap: 10px;
            margin-bottom: 15px;
        }
        .kin-btn {
            flex: 1;
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.2s;
            box-shadow: 0 3px 8px rgba(0,0,0,0.2);
            color: white;
        }
        .kin-btn:active { transform: scale(0.98); }
        .kin-btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        .kin-btn.standup {
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
        }
        .kin-btn.stop {
            background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%);
        }
        
        /* Status */
        .status {
            text-align: center;
            padding: 8px;
            border-radius: 8px;
            background: #f0f0f0;
            font-size: 12px;
            color: #333;
        }
        .status.connected { background: #d4edda; color: #155724; }
        .status.error { background: #f8d7da; color: #721c24; }
        .status.warning { background: #fff3cd; color: #856404; }
    </style>
</head>
<body>
    <div class="container">
        <div class="panel">
            <h1>🤖 Bobot Control</h1>
            <div class="subtitle">Integrated Control Interface</div>
            <div class="status" id="status">Connecting...</div>
        </div>
        
        <div class="panel">
            <h2>Control Buttons</h2>
            <div class="button-grid">
)HTML";

static const char* HTML_PAGE_PART2 = R"HTML(
                <button class="btn btn-0" data-btn="0"><span>⬅️</span><span class="btn-label">BACK</span></button>
                <button class="btn btn-1" data-btn="1"><span>⬆️</span><span class="btn-label">UP</span></button>
                <button class="btn btn-2" data-btn="2"><span>📱</span><span class="btn-label">UI</span></button>
                <button class="btn btn-3" data-btn="3"><span>◀️</span><span class="btn-label">LEFT</span></button>
                <button class="btn btn-4" data-btn="4"><span>⭕</span><span class="btn-label">OK</span></button>
                <button class="btn btn-5" data-btn="5"><span>▶️</span><span class="btn-label">RIGHT</span></button>
                <button class="btn btn-6" data-btn="6"><span>⚙️</span><span class="btn-label">SETTINGS</span></button>
                <button class="btn btn-7" data-btn="7"><span>⬇️</span><span class="btn-label">DOWN</span></button>
                <button class="btn btn-8" data-btn="8"><span>🐛</span><span class="btn-label">DEBUG</span></button>
            </div>
        </div>
        
        <div class="panel">
            <h2>Movement Sequences</h2>
            <div class="kinematic-controls">
                <button class="kin-btn standup" id="standupBtn">▲ Stand Up</button>
                <button class="kin-btn stop" id="stopBtn">■ Stop</button>
            </div>
            <div class="kinematic-controls" style="margin-top: 10px;">
                <button class="kin-btn" id="housingBtn" style="background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);">🏠 Housing</button>
            </div>
        </div>
        
        <div class="panel">
            <h2>Servo Control</h2>
            <div class="speed-control">
                <div class="speed-label">
                    <span>Rotation Speed (°/s)</span>
                    <span class="servo-value" id="speedValue">300</span>
                </div>
                <input type="range" id="speedSlider" min="60" max="600" step="60" value="300">
            </div>
            
            <div class="leg-grid">
                <div class="leg-card">
                    <div class="leg-title">🦵 Back Left (Ch 0-2)</div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch0 - Joint 1</span><span class="servo-value" id="val0">90</span></div>
                        <input type="range" class="servo-slider" data-ch="0" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch1 - Joint 2</span><span class="servo-value" id="val1">90</span></div>
                        <input type="range" class="servo-slider" data-ch="1" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch2 - Joint 3</span><span class="servo-value" id="val2">90</span></div>
                        <input type="range" class="servo-slider" data-ch="2" min="0" max="180" value="90">
                    </div>
                </div>
                
                <div class="leg-card">
                    <div class="leg-title">🦵 Front Left (Ch 3-5)</div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch3 - Joint 1</span><span class="servo-value" id="val3">90</span></div>
                        <input type="range" class="servo-slider" data-ch="3" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch4 - Joint 2</span><span class="servo-value" id="val4">90</span></div>
                        <input type="range" class="servo-slider" data-ch="4" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch5 - Joint 3</span><span class="servo-value" id="val5">90</span></div>
                        <input type="range" class="servo-slider" data-ch="5" min="0" max="180" value="90">
                    </div>
                </div>
                
                <div class="leg-card">
                    <div class="leg-title">🦵 Front Right (Ch 6-8)</div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch6 - Joint 1</span><span class="servo-value" id="val6">90</span></div>
                        <input type="range" class="servo-slider" data-ch="6" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch7 - Joint 2</span><span class="servo-value" id="val7">90</span></div>
                        <input type="range" class="servo-slider" data-ch="7" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch8 - Joint 3</span><span class="servo-value" id="val8">90</span></div>
                        <input type="range" class="servo-slider" data-ch="8" min="0" max="180" value="90">
                    </div>
                </div>
                
                <div class="leg-card">
                    <div class="leg-title">🦵 Back Right (Ch 9-11)</div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch9 - Joint 1</span><span class="servo-value" id="val9">90</span></div>
                        <input type="range" class="servo-slider" data-ch="9" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch10 - Joint 2</span><span class="servo-value" id="val10">90</span></div>
                        <input type="range" class="servo-slider" data-ch="10" min="0" max="180" value="90">
                    </div>
                    <div class="servo-control">
                        <div class="servo-label"><span>Ch11 - Joint 3</span><span class="servo-value" id="val11">90</span></div>
                        <input type="range" class="servo-slider" data-ch="11" min="0" max="180" value="90">
                    </div>
                </div>
            </div>
        </div>
    </div>
)HTML";

static const char* HTML_PAGE_PART3 = R"HTML(
    <script>
        const statusDiv = document.getElementById('status');
        let activeButtons = new Set();
        let sequenceRunning = false;
        let currentSpeed = 300;

        function updateStatus(msg, type = '') {
            statusDiv.textContent = msg;
            statusDiv.className = 'status ' + type;
        }

        // Button controls
        document.querySelectorAll('.btn').forEach(btn => {
            const btnId = parseInt(btn.dataset.btn);
            const events = {
                start: (e) => {
                    e.preventDefault();
                    if (!activeButtons.has(btnId)) {
                        activeButtons.add(btnId);
                        btn.classList.add('pressed');
                        fetch('/api/button', {
                            method: 'POST',
                            headers: {'Content-Type': 'application/json'},
                            body: JSON.stringify({button: btnId, pressed: true})
                        });
                    }
                },
                end: (e) => {
                    e.preventDefault();
                    if (activeButtons.has(btnId)) {
                        activeButtons.delete(btnId);
                        btn.classList.remove('pressed');
                        fetch('/api/button', {
                            method: 'POST',
                            headers: {'Content-Type': 'application/json'},
                            body: JSON.stringify({button: btnId, pressed: false})
                        });
                    }
                }
            };
            btn.addEventListener('mousedown', events.start);
            btn.addEventListener('mouseup', events.end);
            btn.addEventListener('mouseleave', events.end);
            btn.addEventListener('touchstart', events.start);
            btn.addEventListener('touchend', events.end);
            btn.addEventListener('touchcancel', events.end);
        });

        // Servo sliders
        document.querySelectorAll('.servo-slider').forEach(slider => {
            const ch = parseInt(slider.dataset.ch);
            slider.addEventListener('input', (e) => {
                const angle = parseInt(e.target.value);
                document.getElementById('val' + ch).textContent = angle;
                if (!sequenceRunning) {
                    fetch('/api/servo', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({channel: ch, angle: angle, speed: currentSpeed})
                    });
                }
            });
        });

        // Speed control
        document.getElementById('speedSlider').addEventListener('input', (e) => {
            currentSpeed = parseInt(e.target.value);
            document.getElementById('speedValue').textContent = currentSpeed;
            fetch('/api/speed', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({speed: currentSpeed})
            });
        });

        // Housing button
        document.getElementById('housingBtn').addEventListener('click', () => {
            fetch('/api/kinematic', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: 'housing'})
            }).then(r => r.json()).then(data => {
                if (data.success) {
                    updateStatus('Housing sequence started...', 'warning');
                }
            });
        });

        // Stand-up button
        document.getElementById('standupBtn').addEventListener('click', () => {
            fetch('/api/kinematic', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: 'standup'})
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    sequenceRunning = true;
                    updateStatus('Stand-up sequence running...', 'warning');
                    document.querySelectorAll('.servo-slider').forEach(s => s.disabled = true);
                }
            });
        });

        // Stop button
        document.getElementById('stopBtn').addEventListener('click', () => {
            fetch('/api/kinematic', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: 'stop'})
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    sequenceRunning = false;
                    updateStatus('Sequence stopped', 'connected');
                    document.querySelectorAll('.servo-slider').forEach(s => s.disabled = false);
                    updateServoValues();
                }
            });
        });

        // Update servo values from robot
        function updateServoValues() {
            fetch('/api/servo/state')
                .then(r => r.json())
                .then(data => {
                    if (data.angles) {
                        data.angles.forEach((angle, ch) => {
                            if (ch < 12) {
                                document.getElementById('val' + ch).textContent = angle;
                                document.querySelector(`[data-ch="${ch}"]`).value = angle;
                            }
                        });
                    }
                    if (data.running !== undefined) {
                        sequenceRunning = data.running;
                        document.querySelectorAll('.servo-slider').forEach(s => s.disabled = sequenceRunning);
                        if (sequenceRunning) {
                            updateStatus('Stand-up sequence active...', 'warning');
                        }
                    }
                });
        }

        // Periodic updates
        setInterval(updateServoValues, 500);

        // Initial connection
        setTimeout(() => {
            fetch('/api/state')
                .then(r => r.json())
                .then(() => {
                    updateStatus('Connected to Bobot!', 'connected');
                    updateServoValues();
                })
                .catch(() => updateStatus('Connection error!', 'error'));
        }, 500);

        document.addEventListener('contextmenu', e => e.preventDefault());
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

void WebServer::setServoDriver(ServoDriver* driver) {
    servo_driver = driver;
    if (driver) {
        ESP_LOGI(TAG, "Servo driver set to address: %p", (void*)driver);
    } else {
        ESP_LOGW(TAG, "Servo driver set to nullptr");
    }
}

void WebServer::setKinematic(Kinematic* kin) {
    kinematic = kin;
    ESP_LOGI(TAG, "Kinematic controller set");
}

esp_err_t WebServer::initWiFi() {
    // Create AP network interface
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    (void)ap_netif;  // Suppress unused variable warning
    
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
    server_config.max_uri_handlers = 16;  // Increased for more endpoints
    server_config.core_id = config.core_id;  // Run on specified core
    server_config.task_priority = 5;
    server_config.stack_size = 12288;  // Increased stack size
    
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
        
        httpd_uri_t servo_uri = {
            .uri = "/api/servo",
            .method = HTTP_POST,
            .handler = servoHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &servo_uri);
        
        httpd_uri_t servo_state_uri = {
            .uri = "/api/servo/state",
            .method = HTTP_GET,
            .handler = servoStateHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &servo_state_uri);
        
        httpd_uri_t speed_uri = {
            .uri = "/api/speed",
            .method = HTTP_POST,
            .handler = speedHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &speed_uri);
        
        httpd_uri_t kinematic_uri = {
            .uri = "/api/kinematic",
            .method = HTTP_POST,
            .handler = kinematicHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &kinematic_uri);
        
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
}

esp_err_t WebServer::rootHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    // Send HTML in parts to avoid buffer overflow
    httpd_resp_sendstr_chunk(req, HTML_PAGE_PART1);
    httpd_resp_sendstr_chunk(req, HTML_PAGE_PART2);
    httpd_resp_sendstr_chunk(req, HTML_PAGE_PART3);
    httpd_resp_sendstr_chunk(req, nullptr);  // End chunked response
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

esp_err_t WebServer::servoHandler(httpd_req_t* req) {
    char buf[200];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to receive request data");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "Servo request received: %s", buf);
    
    if (!servo_driver) {
        ESP_LOGE(TAG, "Servo driver is null!");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Servo driver not initialized\"}");
        return ESP_OK;
    }
    
    // Parse JSON: {"channel": N, "angle": A, "speed": S}
    int channel = -1;
    int angle = -1;
    float speed = servo_speed;
    
    char* ch_ptr = strstr(buf, "\"channel\"");
    char* angle_ptr = strstr(buf, "\"angle\"");
    char* speed_ptr = strstr(buf, "\"speed\"");
    
    if (ch_ptr) {
        char* colon = strchr(ch_ptr, ':');
        if (colon) channel = atoi(colon + 1);
    }
    
    if (angle_ptr) {
        char* colon = strchr(angle_ptr, ':');
        if (colon) angle = atoi(colon + 1);
    }
    
    if (speed_ptr) {
        char* colon = strchr(speed_ptr, ':');
        if (colon) speed = atof(colon + 1);
    }
    
    ESP_LOGI(TAG, "Parsed values - Channel: %d, Angle: %d, Speed: %.1f", channel, angle, speed);
    
    if (channel >= 0 && channel < 12 && angle >= 0 && angle <= 180) {
        // Apply reverse channel mapping to match local servo mode: UI channel N -> PCA9685 channel (15 - N)
        uint8_t actualChannel = 15 - channel;
        ESP_LOGI(TAG, "Setting UI channel %d (PCA9685 ch %d) to %d degrees at %.1f deg/s", channel, actualChannel, angle, speed);
        esp_err_t err = servo_driver->setAngleSmooth(actualChannel, angle, speed);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Servo command successful");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": true}");
        } else {
            ESP_LOGE(TAG, "Servo command failed with error: %s", esp_err_to_name(err));
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Servo command failed\"}");
        }
    } else {
        ESP_LOGW(TAG, "Invalid parameters - ch:%d angle:%d (must be ch:0-11, angle:0-180)", channel, angle);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Invalid parameters\"}");
    }
    
    return ESP_OK;
}

esp_err_t WebServer::servoStateHandler(httpd_req_t* req) {
    char response[512];
    
    if (!servo_driver) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\": \"Servo driver not initialized\"}");
        return ESP_OK;
    }
    
    // Get current angles for all 12 servos (apply reverse mapping)
    int offset = snprintf(response, sizeof(response), "{\"angles\": [");
    for (int i = 0; i < 12; i++) {
        // UI channel i -> PCA9685 channel (15 - i)
        uint8_t actualChannel = 15 - i;
        offset += snprintf(response + offset, sizeof(response) - offset, 
                          "%d%s", servo_driver->getCurrentAngle(actualChannel), (i < 11) ? "," : "");
    }
    
    // Add running status if kinematic is available
    if (kinematic) {
        offset += snprintf(response + offset, sizeof(response) - offset,
                          "], \"running\": %s}", kinematic->isSequenceRunning() ? "true" : "false");
    } else {
        offset += snprintf(response + offset, sizeof(response) - offset, "], \"running\": false}");
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    
    return ESP_OK;
}

esp_err_t WebServer::speedHandler(httpd_req_t* req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse JSON: {"speed": S}
    char* speed_ptr = strstr(buf, "\"speed\"");
    if (speed_ptr) {
        char* colon = strchr(speed_ptr, ':');
        if (colon) {
            float speed = atof(colon + 1);
            if (speed >= 60.0f && speed <= 600.0f) {
                servo_speed = speed;
                ESP_LOGI(TAG, "Servo speed set to %.0f deg/s", servo_speed);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_sendstr(req, "{\"success\": true}");
                return ESP_OK;
            }
        }
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Invalid speed\"}");
    return ESP_OK;
}

esp_err_t WebServer::kinematicHandler(httpd_req_t* req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    if (!kinematic) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Kinematic controller not initialized\"}");
        return ESP_OK;
    }
    
    // Parse JSON: {"command": "standup" or "stop" or "housing"}
    if (strstr(buf, "\"standup\"")) {
        bool started = kinematic->startStandUpSequence(0.1f);  // 5x slower (60 deg/s)
        if (started) {
            ESP_LOGI(TAG, "Stand-up sequence started");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": true, \"command\": \"standup\"}");
        } else {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Sequence already running\"}");
        }
    } else if (strstr(buf, "\"housing\"")) {
        bool started = kinematic->startHousingSequence();
        if (started) {
            ESP_LOGI(TAG, "Housing sequence started");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": true, \"command\": \"housing\"}");
        } else {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Could not start sequence\"}");
        }
    } else if (strstr(buf, "\"stop\"")) {
        kinematic->stopSequence();
        ESP_LOGI(TAG, "Sequence stopped");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": true, \"command\": \"stop\"}");
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"Invalid command\"}");
    }
    
    return ESP_OK;
}

} // namespace Bobot
