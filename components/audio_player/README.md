# Audio Player Component

Stereo audio player with DMA and ping-pong buffering for ESP32.

## Features

- **Stereo WAV playback** from SD card (44.1kHz, 16-bit, stereo)
- **DMA transfers** for SD card → RAM → I2S pipeline
- **Ping-pong buffering** for near-zero CPU load during playback
- **Runs on Core 0** - leaves Core 1 free for main application
- **Event-driven** - sleeps when not playing audio
- **ISR-safe trigger** - can be called from button interrupts

## Hardware Configuration

Based on [README.md](../../README.md#max98357a-audio-module):

| Signal | GPIO | Description |
|--------|------|-------------|
| I2S_BCLK | GPIO26 | Bit clock |
| I2S_LRC | GPIO27 | Left/Right clock (word select) |
| I2S_DIN | GPIO25 | Serial data output |

Two MAX98357A modules:
- Left channel: SD_MODE tied to VCC
- Right channel: SD_MODE tied to GND

## Usage Example

```cpp
#include "audio_player.hpp"

// Initialize
Bobot::AudioPlayer audioPlayer;
Bobot::AudioPlayer::Config config = {
    .i2s_bclk_pin = 26,
    .i2s_lrc_pin = 27,
    .i2s_dout_pin = 25,
    .sample_rate = 44100,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .ping_pong_buf_size = 8192
};

audioPlayer.init(config);
audioPlayer.start();

// Set trigger file
audioPlayer.setTriggerFile("/sdcard/assets/audio/meow.wav");

// Trigger from button ISR or timer
audioPlayer.triggerPlayback();
```

## Architecture

### Ping-Pong Buffer Strategy

```
┌─────────────┐
│   SD Card   │
│  meow.wav   │
└──────┬──────┘
       │ DMA read (background)
       ▼
┌─────────────────────────────┐
│    Ping-Pong Buffers        │
│  ┌───────┐     ┌───────┐   │
│  │ PING  │ ◄──►│ PONG  │   │
│  │ 8KB   │     │ 8KB   │   │
│  └───┬───┘     └───┬───┘   │
└──────┼─────────────┼────────┘
       │             │
       │ One loads while
       │ other plays
       ▼
┌─────────────────────────────┐
│   I2S DMA (ESP32 hardware)  │
│   ┌───┐ ┌───┐ ┌───┐ ┌───┐  │
│   │ 1 │ │ 2 │ │ 3 │ │ 4 │  │ 4x1KB buffers
│   └───┘ └───┘ └───┘ └───┘  │
└──────────────┬──────────────┘
               ▼
       ┌───────────────┐
       │  MAX98357A    │
       │  Left+Right   │
       └───────────────┘
```

### Task Flow

1. Task sleeps on Core 0 waiting for event
2. Button press sets PLAY_BIT in event group
3. Task wakes, opens WAV file, validates header
4. Pre-loads both ping and pong buffers
5. Starts I2S DMA transfer from ping buffer
6. While ping plays, loads pong from SD card
7. Swaps buffers, repeats until EOF
8. Closes file and returns to sleep

### CPU Load

- **During playback**: ~1-2% on Core 0 (buffer swaps only)
- **When idle**: 0% (task blocked on event group)
- **SD card reads**: DMA-based, minimal CPU involvement
- **I2S output**: Hardware DMA, zero CPU after setup

## File Format Requirements

Only supports WAV files with:
- Format: PCM (audio_format = 1)
- Channels: 2 (stereo)
- Sample rate: 44100 Hz (configurable)
- Bit depth: 16-bit signed integer

Verify with:
```bash
file meow.wav
# meow.wav: RIFF (little-endian) data, WAVE audio, Microsoft PCM, 16 bit, stereo 44100
```

## Integration with Button Driver

The settings button is on MCP23017 at GPA6. To detect a 10ms press:

```cpp
// In button interrupt handler or polling loop
static uint32_t press_start_ms = 0;

void onSettingsButtonPressed() {
    press_start_ms = millis();
}

void onSettingsButtonReleased() {
    uint32_t press_duration = millis() - press_start_ms;
    if (press_duration >= 10 && press_duration < 1000) {
        // Trigger audio (ISR-safe)
        audioPlayer.triggerPlayback();
    }
}
```

## Memory Usage

- **Ping-pong buffers**: 16 KB (2 × 8 KB) in DMA-capable RAM
- **I2S DMA buffers**: 8 KB (4 × 1024 samples × 2 bytes × 2 channels)
- **Task stack**: 8 KB
- **Total**: ~32 KB

## Error Handling

- Invalid WAV format: Returns `ESP_ERR_NOT_SUPPORTED`
- File not found: Returns `ESP_ERR_NOT_FOUND`
- Memory allocation failure: Returns `ESP_ERR_NO_MEM`
- I2S errors: Logged and playback stopped

## Performance Notes

- Uses `heap_caps_malloc(MALLOC_CAP_DMA)` for I2S-compatible buffers
- File reads are buffered by FatFS layer
- No floating-point operations in audio path
- Task pinned to Core 0 for deterministic timing

## Future Enhancements

- [ ] Volume control
- [ ] Multiple simultaneous sounds (mixing)
- [ ] Fade in/out effects
- [ ] Support for MP3/AAC with software decoder
- [ ] Playlist support
- [ ] Pause/resume functionality
