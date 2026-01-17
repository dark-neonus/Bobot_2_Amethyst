/**
 * @file audio_player.hpp
 * @brief Stereo audio player with DMA and ping-pong buffering
 * 
 * Features:
 * - Plays stereo WAV files from SD card
 * - Uses DMA for SD card → RAM → I2S transfer
 * - Ping-pong buffer strategy for minimal CPU load
 * - Runs on ESP32 core 0
 * - Triggered by settings button (10ms press)
 */

#pragma once

#include <esp_err.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <cstdint>

namespace Bobot {

/**
 * @brief Audio player class with ping-pong DMA buffering
 */
class AudioPlayer {
public:
    /**
     * @brief Configuration for audio player
     */
    struct Config {
        int i2s_bclk_pin;      ///< Bit clock pin (GPIO26)
        int i2s_lrc_pin;       ///< Word select pin (GPIO27)
        int i2s_dout_pin;      ///< Data out pin (GPIO25)
        uint32_t sample_rate;  ///< Sample rate in Hz (44100)
        size_t dma_buf_count;  ///< Number of DMA buffers (4)
        size_t dma_buf_len;    ///< Length of each DMA buffer in samples (1024)
        size_t ping_pong_buf_size; ///< Size of each ping-pong buffer in bytes (8192)
    };

    /**
     * @brief Constructor
     */
    AudioPlayer();

    /**
     * @brief Destructor
     */
    ~AudioPlayer();

    /**
     * @brief Initialize audio player with configuration
     * @param config Configuration structure
     * @return ESP_OK on success
     */
    esp_err_t init(const Config& config);

    /**
     * @brief Start audio player task on core 0
     * @return ESP_OK on success
     */
    esp_err_t start();

    /**
     * @brief Play audio file from SD card
     * @param filepath Path to WAV file on SD card
     * @return ESP_OK on success
     */
    esp_err_t play(const char* filepath);

    /**
     * @brief Stop playback
     */
    void stop();

    /**
     * @brief Check if audio is currently playing
     * @return true if playing
     */
    bool isPlaying() const;

    /**
     * @brief Trigger playback (called from ISR or button handler)
     * This is ISR-safe and uses event groups to wake the task
     */
    void triggerPlayback();

    /**
     * @brief Set the audio file path to play when triggered
     * @param filepath Path to WAV file
     */
    void setTriggerFile(const char* filepath);

private:
    // WAV file header structure
    struct __attribute__((packed)) WavHeader {
        // RIFF chunk
        char riff[4];           // "RIFF"
        uint32_t file_size;     // File size - 8
        char wave[4];           // "WAVE"
        
        // fmt chunk
        char fmt[4];            // "fmt "
        uint32_t fmt_size;      // Size of fmt chunk
        uint16_t audio_format;  // 1 = PCM
        uint16_t num_channels;  // 1 = mono, 2 = stereo
        uint32_t sample_rate;   // Samples per second
        uint32_t byte_rate;     // Bytes per second
        uint16_t block_align;   // Bytes per sample (all channels)
        uint16_t bits_per_sample; // Bits per sample
        
        // data chunk
        char data[4];           // "data"
        uint32_t data_size;     // Size of audio data
    };

    // Ping-pong buffer indices
    enum BufferIndex {
        PING = 0,
        PONG = 1
    };

    /**
     * @brief Audio player task (runs on core 0)
     */
    static void audioTaskStatic(void* arg);
    void audioTask();

    /**
     * @brief Load next chunk from SD card to buffer
     * @param buffer_idx Ping or Pong buffer index
     * @return Number of bytes read, 0 on EOF
     */
    size_t loadChunk(BufferIndex buffer_idx);

    /**
     * @brief Parse and validate WAV header
     * @param header Header structure to fill
     * @return ESP_OK if valid
     */
    esp_err_t parseWavHeader(WavHeader& header);

    // Configuration
    Config config_;
    
    // I2S channel handle
    i2s_chan_handle_t i2s_tx_chan_;
    
    // Task handle
    TaskHandle_t task_handle_;
    
    // Event group for synchronization
    EventGroupHandle_t event_group_;
    static constexpr int PLAY_BIT = BIT0;
    static constexpr int STOP_BIT = BIT1;
    static constexpr int CHUNK_LOADED_BIT = BIT2;
    
    // Ping-pong buffers
    uint8_t* buffers_[2];
    volatile size_t buffer_bytes_[2];
    volatile BufferIndex current_load_buffer_;
    volatile BufferIndex current_play_buffer_;
    
    // File handling
    FILE* audio_file_;
    char trigger_filepath_[256];
    size_t bytes_remaining_;
    volatile bool playing_;
    
    // RAM-based playback (for slow SD cards)
    uint8_t* full_audio_buffer_;    ///< Full audio file loaded into RAM
    size_t full_audio_size_;        ///< Size of full audio buffer in bytes
    size_t ram_playback_offset_;    ///< Current read offset in RAM buffer
    bool is_mono_;                  ///< True if loaded file is mono (will be duplicated to stereo)
    
    // Statistics
    uint32_t underruns_;
};

} // namespace Bobot
