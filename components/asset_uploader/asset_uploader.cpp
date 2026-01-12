#include "asset_uploader.hpp"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static const char* TAG = "AssetUploader";

namespace Bobot {

AssetUploader::AssetUploader(SDCard* sd_card, const char* wifi_ssid, const char* wifi_password)
    : sd_card_(sd_card)
    , wifi_ssid_(wifi_ssid)
    , wifi_password_(wifi_password)
    , server_(nullptr)
    , active_(false)
    , uploading_(false)
    , progress_(0)
{
}

AssetUploader::~AssetUploader() {
    stop();
}

bool AssetUploader::start() {
    if (active_) {
        ESP_LOGW(TAG, "Uploader already active");
        return true;
    }

    if (!sd_card_ || !sd_card_->isMounted()) {
        ESP_LOGE(TAG, "SD card not available");
        return false;
    }

    ESP_LOGI(TAG, "Starting asset upload mode...");

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Configure WiFi AP
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, wifi_ssid_.c_str(), sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char*)wifi_config.ap.password, wifi_password_.c_str(), sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    
    if (strlen(wifi_password_.c_str()) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started - SSID: %s", wifi_ssid_.c_str());
    ESP_LOGI(TAG, "Connect to WiFi and use IP: 192.168.4.1");

    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.ctrl_port = 8081;
    config.max_uri_handlers = 8;
    config.max_open_sockets = 4;
    config.stack_size = 8192;  // Increased from default 4096
    config.recv_wait_timeout = 30;  // 30 second timeout for receiving data
    config.send_wait_timeout = 30;  // 30 second timeout for sending data
    config.lru_purge_enable = true;
    
    ret = httpd_start(&server_, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        esp_wifi_stop();
        return false;
    }

    // Register URI handlers
    httpd_uri_t start_uri = {
        .uri = "/start",
        .method = HTTP_POST,
        .handler = start_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &start_uri);

    httpd_uri_t file_uri = {
        .uri = "/file",
        .method = HTTP_POST,
        .handler = file_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &file_uri);

    httpd_uri_t complete_uri = {
        .uri = "/complete",
        .method = HTTP_POST,
        .handler = complete_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &complete_uri);

    active_ = true;
    ESP_LOGI(TAG, "Asset upload mode active on port 8080");
    ESP_LOGI(TAG, "Connect to WiFi: %s", wifi_ssid_.c_str());
    
    return true;
}

void AssetUploader::stop() {
    if (!active_) {
        return;
    }

    ESP_LOGI(TAG, "Stopping asset upload mode...");

    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    active_ = false;
    uploading_ = false;
    progress_ = 0;

    ESP_LOGI(TAG, "Asset upload mode stopped");
}

void AssetUploader::completeUpload() {
    uploading_ = false;
    progress_ = 100;
    
    // Give a short delay then stop
    vTaskDelay(pdMS_TO_TICKS(1000));
    stop();
}

esp_err_t AssetUploader::start_handler(httpd_req_t* req) {
    AssetUploader* uploader = (AssetUploader*)req->user_ctx;
    
    ESP_LOGI(TAG, "Received /start request");
    
    uploader->uploading_ = true;
    uploader->progress_ = 0;
    
    // Delete /assets directory
    std::string assets_path = std::string(uploader->sd_card_->getMountPoint()) + "/assets";
    
    if (uploader->deleteDirectory(assets_path.c_str())) {
        ESP_LOGI(TAG, "Cleared /assets directory");
        
        // Send success response
        const char* resp = "{\"status\":\"ok\",\"message\":\"Ready for upload\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to clear /assets directory");
        
        const char* resp = "{\"status\":\"error\",\"message\":\"Failed to clear assets\"}";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }
}

esp_err_t AssetUploader::file_handler(httpd_req_t* req) {
    AssetUploader* uploader = (AssetUploader*)req->user_ctx;
    
    if (!uploader->uploading_) {
        const char* resp = "{\"status\":\"error\",\"message\":\"Upload not started. Call /start first\"}";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }
    
    // Get file path from query parameter
    char path_buf[256];
    if (httpd_req_get_url_query_str(req, path_buf, sizeof(path_buf)) != ESP_OK) {
        const char* resp = "{\"status\":\"error\",\"message\":\"Missing path parameter\"}";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }
    
    char path_param[256];
    if (httpd_query_key_value(path_buf, "path", path_param, sizeof(path_param)) != ESP_OK) {
        const char* resp = "{\"status\":\"error\",\"message\":\"Missing path parameter\"}";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Uploading file: %s", path_param);
    
    // Build full path
    std::string full_path = std::string(uploader->sd_card_->getMountPoint()) + "/assets/" + path_param;
    
    // Ensure directory exists
    size_t last_slash = full_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string dir_path = full_path.substr(0, last_slash);
        uploader->ensureDirectoryExists(dir_path.c_str());
    }
    
    // Open file for writing
    FILE* file = fopen(full_path.c_str(), "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path.c_str());
        const char* resp = "{\"status\":\"error\",\"message\":\"Failed to open file\"}";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_FAIL;
    }
    
    // Read and write data in chunks
    char buf[512];  // Smaller buffer to reduce memory pressure
    int remaining = req->content_len;
    int received = 0;
    
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, std::min(remaining, (int)sizeof(buf)));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(file);
            ESP_LOGE(TAG, "Failed to receive data: %d", recv_len);
            const char* resp = "{\"status\":\"error\",\"message\":\"Failed to receive data\"}";
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, strlen(resp));
            return ESP_FAIL;
        }
        
        if (fwrite(buf, 1, recv_len, file) != (size_t)recv_len) {
            fclose(file);
            ESP_LOGE(TAG, "Failed to write to SD card");
            const char* resp = "{\"status\":\"error\",\"message\":\"Failed to write to SD card\"}";
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, strlen(resp));
            return ESP_FAIL;
        }
        
        remaining -= recv_len;
        received += recv_len;
        
        // Yield to other tasks periodically
        if (received % 2048 == 0) {
            taskYIELD();
        }
    }
    
    fclose(file);
    
    ESP_LOGI(TAG, "File uploaded successfully: %s (%d bytes)", path_param, received);
    
    // Send success response
    char resp_buf[128];
    snprintf(resp_buf, sizeof(resp_buf), "{\"status\":\"ok\",\"message\":\"File uploaded\",\"bytes\":%d}", received);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_buf, strlen(resp_buf));
    
    return ESP_OK;
}

esp_err_t AssetUploader::complete_handler(httpd_req_t* req) {
    AssetUploader* uploader = (AssetUploader*)req->user_ctx;
    
    ESP_LOGI(TAG, "Upload complete");
    
    uploader->uploading_ = false;
    uploader->progress_ = 100;
    
    const char* resp = "{\"status\":\"ok\",\"message\":\"Upload complete\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    
    // Schedule uploader stop (give time for response to be sent)
    ESP_LOGI(TAG, "Scheduling upload mode shutdown...");
    
    return ESP_OK;
}

bool AssetUploader::deleteDirectory(const char* path) {
    DIR* dir = opendir(path);
    if (dir == nullptr) {
        ESP_LOGW(TAG, "Directory does not exist, nothing to delete: %s", path);
        return true; // Not an error if it doesn't exist
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string entry_path = std::string(path) + "/" + entry->d_name;
        
        struct stat st;
        if (stat(entry_path.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively delete subdirectory
                deleteDirectory(entry_path.c_str());
            } else {
                // Delete file
                if (unlink(entry_path.c_str()) != 0) {
                    ESP_LOGE(TAG, "Failed to delete file: %s", entry_path.c_str());
                    closedir(dir);
                    return false;
                }
                ESP_LOGD(TAG, "Deleted file: %s", entry_path.c_str());
            }
        }
    }
    
    closedir(dir);
    
    // Delete the directory itself
    if (rmdir(path) != 0) {
        ESP_LOGE(TAG, "Failed to delete directory: %s", path);
        return false;
    }
    
    ESP_LOGD(TAG, "Deleted directory: %s", path);
    return true;
}

bool AssetUploader::ensureDirectoryExists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
    // Try to create directory
    std::string path_str(path);
    size_t pos = 0;
    
    while ((pos = path_str.find('/', pos + 1)) != std::string::npos) {
        std::string subpath = path_str.substr(0, pos);
        if (stat(subpath.c_str(), &st) != 0) {
            if (mkdir(subpath.c_str(), 0755) != 0) {
                ESP_LOGE(TAG, "Failed to create directory: %s", subpath.c_str());
                return false;
            }
        }
    }
    
    // Create final directory
    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", path);
        return false;
    }
    
    return true;
}

} // namespace Bobot
