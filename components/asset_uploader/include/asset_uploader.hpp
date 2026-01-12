#ifndef ASSET_UPLOADER_HPP
#define ASSET_UPLOADER_HPP

#include <esp_http_server.h>
#include <string>
#include "sd_card.hpp"

namespace Bobot {

/**
 * @brief Asset uploader service for updating SD card assets via WiFi
 * 
 * This class provides an HTTP server that allows remote asset uploads
 * to the SD card. It runs on port 8080 and provides three endpoints:
 * - POST /start - Clears the /assets directory
 * - POST /file - Uploads a file to the SD card
 * - POST /complete - Finalizes upload and shuts down server
 */
class AssetUploader {
public:
    /**
     * @brief Construct a new Asset Uploader
     * 
     * @param sd_card Pointer to initialized SD card instance
     * @param wifi_ssid WiFi SSID for AP mode
     * @param wifi_password WiFi password for AP mode
     */
    AssetUploader(SDCard* sd_card, const char* wifi_ssid = "Bobot_Upload", const char* wifi_password = "bobot123");
    
    /**
     * @brief Destroy the Asset Uploader
     */
    ~AssetUploader();

    /**
     * @brief Start upload mode
     * 
     * This will:
     * 1. Start WiFi in AP mode (IP: 192.168.4.1)
     * 2. Start HTTP server on port 8080
     * 
     * @return true if started successfully
     * @return false if failed to start
     */
    bool start();

    /**
     * @brief Stop upload mode
     * 
     * This will:
     * 1. Stop HTTP server
     * 2. Stop WiFi
     */
    void stop();
    
    /**
     * @brief Mark upload as complete and schedule shutdown
     */
    void completeUpload();

    /**
     * @brief Check if uploader is active
     * 
     * @return true if active
     * @return false if not active
     */
    bool isActive() const { return active_; }

    /**
     * @brief Check if upload is in progress
     * 
     * @return true if upload in progress
     * @return false if no upload in progress
     */
    bool isUploading() const { return uploading_; }

    /**
     * @brief Get upload progress (0-100%)
     * 
     * @return uint8_t Progress percentage
     */
    uint8_t getProgress() const { return progress_; }

private:
    SDCard* sd_card_;
    std::string wifi_ssid_;
    std::string wifi_password_;
    httpd_handle_t server_;
    bool active_;
    bool uploading_;
    uint8_t progress_;
    
    // HTTP handler functions
    static esp_err_t start_handler(httpd_req_t* req);
    static esp_err_t file_handler(httpd_req_t* req);
    static esp_err_t complete_handler(httpd_req_t* req);
    
    // Helper functions
    bool deleteDirectory(const char* path);
    bool ensureDirectoryExists(const char* path);
};

} // namespace Bobot

#endif // ASSET_UPLOADER_HPP
