/**
 * @file audio_player.cpp
 * @brief Implementation of stereo audio player with DMA and ping-pong buffering
 */

#include "audio_player.hpp"
#include <esp_log.h>
#include <string.h>
#include <algorithm>

static const char* TAG = "AudioPlayer";

namespace Bobot {

AudioPlayer::AudioPlayer()
    : i2s_tx_chan_(nullptr)
    , task_handle_(nullptr)
    , event_group_(nullptr)
    , current_load_buffer_(PING)
    , current_play_buffer_(PONG)
    , audio_file_(nullptr)
    , bytes_remaining_(0)
    , playing_(false)
    , underruns_(0)
{
    buffers_[0] = nullptr;
    buffers_[1] = nullptr;
    buffer_bytes_[0] = 0;
    buffer_bytes_[1] = 0;
    trigger_filepath_[0] = '\0';
    full_audio_buffer_ = nullptr;
    full_audio_size_ = 0;
    ram_playback_offset_ = 0;
    is_mono_ = false;
}

AudioPlayer::~AudioPlayer() {
    stop();
    
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    
    if (event_group_) {
        vEventGroupDelete(event_group_);
        event_group_ = nullptr;
    }
    
    if (i2s_tx_chan_) {
        i2s_channel_disable(i2s_tx_chan_);
        i2s_del_channel(i2s_tx_chan_);
        i2s_tx_chan_ = nullptr;
    }
    
    if (buffers_[0]) {
        free(buffers_[0]);
        buffers_[0] = nullptr;
    }
    
    if (buffers_[1]) {
        free(buffers_[1]);
        buffers_[1] = nullptr;
    }
}

esp_err_t AudioPlayer::init(const Config& config) {
    config_ = config;
    
    // Allocate ping-pong buffers
    buffers_[PING] = (uint8_t*)heap_caps_malloc(config_.ping_pong_buf_size, MALLOC_CAP_DMA);
    buffers_[PONG] = (uint8_t*)heap_caps_malloc(config_.ping_pong_buf_size, MALLOC_CAP_DMA);
    
    if (!buffers_[PING] || !buffers_[PONG]) {
        ESP_LOGE(TAG, "Failed to allocate ping-pong buffers");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Allocated %zu byte ping-pong buffers", config_.ping_pong_buf_size);
    
    // Create event group
    event_group_ = xEventGroupCreate();
    if (!event_group_) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Configure I2S channel in stereo mode
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = (int)config_.dma_buf_count;
    chan_cfg.dma_frame_num = (int)config_.dma_buf_len;
    chan_cfg.auto_clear = true;
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, &i2s_tx_chan_, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure I2S standard mode for stereo Philips/I2S format
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config_.sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = static_cast<gpio_num_t>(config_.i2s_bclk_pin),
            .ws   = static_cast<gpio_num_t>(config_.i2s_lrc_pin),
            .dout = static_cast<gpio_num_t>(config_.i2s_dout_pin),
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    
    // Log stereo configuration
    ESP_LOGI(TAG, "I2S slot config: STEREO mode, both left and right channels enabled");
    
    ret = i2s_channel_init_std_mode(i2s_tx_chan_, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S standard mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Enable I2S channel
    ret = i2s_channel_enable(i2s_tx_chan_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2S initialized: %d Hz, stereo, 16-bit", config_.sample_rate);
    ESP_LOGI(TAG, "Pins: BCLK=%d, LRC=%d, DOUT=%d", 
             config_.i2s_bclk_pin, config_.i2s_lrc_pin, config_.i2s_dout_pin);
    
    return ESP_OK;
}

esp_err_t AudioPlayer::start() {
    if (task_handle_) {
        ESP_LOGW(TAG, "Task already running");
        return ESP_OK;
    }
    
    // Create task on core 0
    BaseType_t ret = xTaskCreatePinnedToCore(
        audioTaskStatic,
        "audio_player",
        8192,              // Stack size
        this,
        5,                 // Priority
        &task_handle_,
        0                  // Pin to core 0
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Audio task started on core 0");
    return ESP_OK;
}

void AudioPlayer::setTriggerFile(const char* filepath) {
    strncpy(trigger_filepath_, filepath, sizeof(trigger_filepath_) - 1);
    trigger_filepath_[sizeof(trigger_filepath_) - 1] = '\0';
    ESP_LOGI(TAG, "Trigger file set: %s", trigger_filepath_);
}

void AudioPlayer::triggerPlayback() {
    ESP_LOGI(TAG, "triggerPlayback() called");
    if (event_group_) {
        EventBits_t bits = xEventGroupSetBits(event_group_, PLAY_BIT);
        ESP_LOGI(TAG, "Event bits set, result: 0x%x", bits);
    } else {
        ESP_LOGE(TAG, "Event group is NULL!");
    }
}

esp_err_t AudioPlayer::play(const char* filepath) {
    if (playing_) {
        ESP_LOGW(TAG, "Already playing, stopping first");
        stop();
    }
    
    // Open WAV file
    audio_file_ = fopen(filepath, "rb");
    if (!audio_file_) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Parse WAV header
    WavHeader header;
    esp_err_t ret = parseWavHeader(header);
    if (ret != ESP_OK) {
        fclose(audio_file_);
        audio_file_ = nullptr;
        return ret;
    }
    
    // Validate format (accept mono or stereo)
    if (header.num_channels != 1 && header.num_channels != 2) {
        ESP_LOGE(TAG, "Only mono or stereo files supported (got %d channels)", header.num_channels);
        fclose(audio_file_);
        audio_file_ = nullptr;
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    is_mono_ = (header.num_channels == 1);
    
    if (header.sample_rate != config_.sample_rate) {
        ESP_LOGW(TAG, "Sample rate mismatch: file=%d, config=%d", 
                 header.sample_rate, config_.sample_rate);
    }
    
    if (header.bits_per_sample != 16) {
        ESP_LOGE(TAG, "Only 16-bit samples supported (got %d)", header.bits_per_sample);
        fclose(audio_file_);
        audio_file_ = nullptr;
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    ESP_LOGI(TAG, "Playing: %s", filepath);
    ESP_LOGI(TAG, "Format: %d Hz, %d ch, %d bit, %d bytes", 
             header.sample_rate, header.num_channels, 
             header.bits_per_sample, header.data_size);
    ESP_LOGI(TAG, "Block align: %d, Byte rate: %d", header.block_align, header.byte_rate);
    
    if (is_mono_) {
        ESP_LOGI(TAG, "Mono file - will duplicate to both channels");
        // Mono: 1 channel * 2 bytes = 2
        if (header.block_align != 2) {
            ESP_LOGW(TAG, "Unexpected block align for mono 16-bit: %d (expected 2)", header.block_align);
        }
    } else {
        ESP_LOGI(TAG, "Stereo file - interleaved format: LRLRLR...");
        // Stereo: 2 channels * 2 bytes = 4
        if (header.block_align != 4) {
            ESP_LOGW(TAG, "Unexpected block align for stereo 16-bit: %d (expected 4)", header.block_align);
        }
    }
    
    // Free any existing RAM buffer from previous playback
    if (full_audio_buffer_) {
        ESP_LOGW(TAG, "Freeing existing audio buffer before allocating new one");
        free(full_audio_buffer_);
        full_audio_buffer_ = nullptr;
        full_audio_size_ = 0;
        ram_playback_offset_ = 0;
    }
    
    // Allocate RAM buffer for entire audio file
    full_audio_size_ = header.data_size;
    full_audio_buffer_ = (uint8_t*)malloc(full_audio_size_);
    if (!full_audio_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for audio buffer", full_audio_size_);
        fclose(audio_file_);
        audio_file_ = nullptr;
        full_audio_size_ = 0;
        return ESP_ERR_NO_MEM;
    }
    
    // Load entire audio file into RAM (slow SD card read, but only once)
    ESP_LOGI(TAG, "Loading %zu bytes into RAM...", full_audio_size_);
    size_t bytes_read = fread(full_audio_buffer_, 1, full_audio_size_, audio_file_);
    fclose(audio_file_);
    audio_file_ = nullptr;
    
    if (bytes_read != full_audio_size_) {
        ESP_LOGE(TAG, "Failed to read audio file: got %zu/%zu bytes", bytes_read, full_audio_size_);
        free(full_audio_buffer_);
        full_audio_buffer_ = nullptr;
        full_audio_size_ = 0;
        ram_playback_offset_ = 0;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Audio loaded into RAM, ready for playback");
    
    bytes_remaining_ = full_audio_size_;
    ram_playback_offset_ = 0;
    playing_ = true;
    
    // Pre-load BOTH buffers from RAM
    current_play_buffer_ = PING;
    current_load_buffer_ = PONG;
    
    buffer_bytes_[PING] = loadChunk(PING);
    if (buffer_bytes_[PING] == 0) {
        ESP_LOGE(TAG, "Failed to load initial audio data into PING");
        playing_ = false;
        // Clean up RAM buffer on error
        if (full_audio_buffer_) {
            free(full_audio_buffer_);
            full_audio_buffer_ = nullptr;
            full_audio_size_ = 0;
            ram_playback_offset_ = 0;
        }
        return ESP_FAIL;
    }
    
    buffer_bytes_[PONG] = loadChunk(PONG);
    if (buffer_bytes_[PONG] == 0) {
        ESP_LOGW(TAG, "File too small for double buffering (only one buffer filled)");
    }
    
    return ESP_OK;
}

void AudioPlayer::stop() {
    if (!playing_) {
        return;
    }
    
    playing_ = false;
    
    if (audio_file_) {
        fclose(audio_file_);
        audio_file_ = nullptr;
    }
    
    // Free RAM buffer
    if (full_audio_buffer_) {
        free(full_audio_buffer_);
        full_audio_buffer_ = nullptr;
        full_audio_size_ = 0;
        ram_playback_offset_ = 0;
    }
    
    bytes_remaining_ = 0;
    
    // Clear I2S buffers by preloading zeros
    if (i2s_tx_chan_) {
        uint8_t zero_buf[128] = {0};
        size_t written;
        i2s_channel_preload_data(i2s_tx_chan_, zero_buf, sizeof(zero_buf), &written);
    }
    
    ESP_LOGI(TAG, "Playback stopped (underruns: %d)", underruns_);
}

bool AudioPlayer::isPlaying() const {
    return playing_;
}

size_t AudioPlayer::loadChunk(BufferIndex buffer_idx) {
    if (!full_audio_buffer_ || bytes_remaining_ == 0) {
        return 0;
    }
    
    if (is_mono_) {
        // Mono: duplicate each sample to both channels (L and R)
        // Read mono samples and write stereo samples
        size_t mono_samples = std::min(static_cast<size_t>(config_.ping_pong_buf_size / 4), bytes_remaining_ / 2);
        size_t mono_bytes = mono_samples * 2;  // 2 bytes per mono sample
        size_t stereo_bytes = mono_samples * 4; // 4 bytes per stereo frame (L+R)
        
        int16_t* src = (int16_t*)(full_audio_buffer_ + ram_playback_offset_);
        int16_t* dst = (int16_t*)buffers_[buffer_idx];
        
        for (size_t i = 0; i < mono_samples; i++) {
            int16_t sample = src[i];
            dst[i * 2] = sample;      // Left channel
            dst[i * 2 + 1] = sample;  // Right channel (duplicate)
        }
        
        ram_playback_offset_ += mono_bytes;
        bytes_remaining_ -= mono_bytes;
        
        return stereo_bytes;
    } else {
        // Stereo: direct copy
        size_t to_read = std::min(static_cast<size_t>(config_.ping_pong_buf_size), bytes_remaining_);
        
        memcpy(buffers_[buffer_idx], full_audio_buffer_ + ram_playback_offset_, to_read);
        
        ram_playback_offset_ += to_read;
        bytes_remaining_ -= to_read;
        
        return to_read;
    }
}

esp_err_t AudioPlayer::parseWavHeader(WavHeader& header) {
    if (!audio_file_) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Read RIFF header
    size_t read = fread(&header, 1, 12, audio_file_);  // Read RIFF, file_size, WAVE
    if (read != 12) {
        ESP_LOGE(TAG, "Failed to read RIFF header");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Validate RIFF
    if (memcmp(header.riff, "RIFF", 4) != 0) {
        ESP_LOGE(TAG, "Invalid RIFF signature");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate WAVE
    if (memcmp(header.wave, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAVE signature");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read fmt chunk
    read = fread(&header.fmt, 1, 24, audio_file_);  // Read fmt header + data
    if (read != 24) {
        ESP_LOGE(TAG, "Failed to read fmt chunk");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Validate fmt
    if (memcmp(header.fmt, "fmt ", 4) != 0) {
        ESP_LOGE(TAG, "Invalid fmt signature");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate PCM format
    if (header.audio_format != 1) {
        ESP_LOGE(TAG, "Only PCM format supported (got %d)", header.audio_format);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Skip any extra bytes in fmt chunk (if fmt_size > 16)
    if (header.fmt_size > 16) {
        fseek(audio_file_, header.fmt_size - 16, SEEK_CUR);
    }
    
    // Search for data chunk (skip unknown chunks like LIST, fact, etc.)
    char chunk_id[4];
    uint32_t chunk_size;
    bool found_data = false;
    
    for (int i = 0; i < 10; i++) {  // Try up to 10 chunks
        read = fread(chunk_id, 1, 4, audio_file_);
        if (read != 4) {
            break;
        }
        
        read = fread(&chunk_size, 1, 4, audio_file_);
        if (read != 4) {
            break;
        }
        
        if (memcmp(chunk_id, "data", 4) == 0) {
            memcpy(header.data, chunk_id, 4);
            header.data_size = chunk_size;
            found_data = true;
            break;
        } else {
            // Skip unknown chunk
            ESP_LOGI(TAG, "Skipping chunk: %.4s (size: %u)", chunk_id, chunk_size);
            fseek(audio_file_, chunk_size, SEEK_CUR);
        }
    }
    
    if (!found_data) {
        ESP_LOGE(TAG, "Data chunk not found");
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

void AudioPlayer::audioTaskStatic(void* arg) {
    AudioPlayer* player = static_cast<AudioPlayer*>(arg);
    player->audioTask();
}

void AudioPlayer::audioTask() {
    ESP_LOGI(TAG, "Audio task running on core %d", xPortGetCoreID());
    
    while (true) {
        // Wait for playback trigger or stop event
        EventBits_t bits = xEventGroupWaitBits(
            event_group_,
            PLAY_BIT | STOP_BIT,
            pdTRUE,  // Clear on exit
            pdFALSE, // Wait for any bit
            portMAX_DELAY
        );
        
        if (bits & STOP_BIT) {
            stop();
            continue;
        }
        
        if (bits & PLAY_BIT) {
            ESP_LOGI(TAG, "PLAY_BIT received, triggering playback");
            if (trigger_filepath_[0] != '\0') {
                ESP_LOGI(TAG, "Attempting to play: %s", trigger_filepath_);
                esp_err_t ret = play(trigger_filepath_);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to play triggered file: %s", esp_err_to_name(ret));
                    continue;
                }
                
                // Ping-pong buffer playback loop
                ESP_LOGI(TAG, "Starting playback loop");
                int iteration = 0;
                while (playing_) {
                    iteration++;
                    
                    // Write current play buffer to I2S DMA (blocks until space available)
                    size_t bytes_written = 0;
                    uint32_t write_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    esp_err_t ret = i2s_channel_write(
                        i2s_tx_chan_, 
                        buffers_[current_play_buffer_], 
                        buffer_bytes_[current_play_buffer_],
                        &bytes_written, 
                        portMAX_DELAY  // Wait as long as needed for DMA space
                    );
                    uint32_t write_time = (xTaskGetTickCount() * portTICK_PERIOD_MS) - write_start;
                    
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "I2S write error: %s", esp_err_to_name(ret));
                        playing_ = false;
                        break;
                    }
                    
                    if (bytes_written != buffer_bytes_[current_play_buffer_]) {
                        ESP_LOGW(TAG, "Partial write: %zu/%zu bytes", bytes_written, buffer_bytes_[current_play_buffer_]);
                    }
                    
                    if (iteration % 20 == 0) {
                        ESP_LOGI(TAG, "Iteration %d: wrote %zu bytes in %dms, %zu bytes remaining", 
                                 iteration, bytes_written, write_time, bytes_remaining_);
                    }
                    
                    // While that buffer plays from DMA, load next chunk into the other buffer
                    size_t bytes_loaded = loadChunk(current_load_buffer_);
                    buffer_bytes_[current_load_buffer_] = bytes_loaded;
                    
                    if (bytes_loaded == 0) {
                        // No more data to load - current buffer will finish playing, then we're done
                        ESP_LOGI(TAG, "Reached end of file after %d iterations", iteration);
                        playing_ = false;
                        break;
                    }
                    
                    // Swap buffers: what we just loaded becomes next play buffer
                    BufferIndex temp = current_play_buffer_;
                    current_play_buffer_ = current_load_buffer_;
                    current_load_buffer_ = temp;
                }
                
                stop();
                ESP_LOGI(TAG, "Playback complete");
            }
        }
    }
}

} // namespace Bobot
