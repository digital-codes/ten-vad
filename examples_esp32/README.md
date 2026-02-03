# TEN VAD for ESP32-S3 with ESP-IDF v5

This directory contains the ESP32-S3 port of TEN VAD using ESP-IDF version 5.

## Hardware Requirements

- **ESP32-S3** microcontroller
- **8MB PSRAM** (external SPIRAM)
- USB connection for programming and serial output

## Software Requirements

- **ESP-IDF v5.0 or later** (tested with v5.x)
- Python 3.8 or later
- CMake 3.16 or later

## Features

- Full TEN VAD functionality on ESP32-S3
- Optimized for 8MB PSRAM usage
- Low latency voice activity detection
- Real-time processing at 16kHz sample rate
- Memory-efficient implementation

## Installation

### 1. Install ESP-IDF v5

Follow the official ESP-IDF installation guide:
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/

```bash
# Example for Linux/macOS
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

### 2. Clone TEN VAD Repository

```bash
git clone https://github.com/TEN-framework/ten-vad.git
cd ten-vad/examples_esp32
```

## Building

### 1. Set the ESP32-S3 Target

```bash
cd examples_esp32
idf.py set-target esp32s3
```

### 2. Configure the Project (Optional)

```bash
idf.py menuconfig
```

The default configuration in `sdkconfig.defaults` is optimized for ESP32-S3 with 8MB PSRAM. Key settings include:
- PSRAM: Octal mode at 80MHz
- PSRAM allocation strategy: Use for large allocations
- C++ exceptions and RTTI: Enabled
- Compiler optimization: Size (-Os)

### 3. Build the Project

```bash
idf.py build
```

## Flashing and Running

### 1. Connect ESP32-S3 Board

Connect your ESP32-S3 board to your computer via USB.

### 2. Flash the Firmware

```bash
idf.py -p /dev/ttyUSB0 flash
```

Replace `/dev/ttyUSB0` with your actual serial port (e.g., `COM3` on Windows, `/dev/cu.usbserial-*` on macOS).

### 3. Monitor Output

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Or combined flash and monitor:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

To exit the monitor, press `Ctrl+]`.

## Expected Output

When running successfully, you should see output similar to:

```
I (XXX) TEN_VAD_ESP32: ===================================
I (XXX) TEN_VAD_ESP32: TEN VAD ESP32-S3 Demo Application
I (XXX) TEN_VAD_ESP32: ===================================
I (XXX) TEN_VAD_ESP32: ESP-IDF Version: v5.x.x
I (XXX) TEN_VAD_ESP32: Free heap at startup: XXXXX bytes
I (XXX) TEN_VAD_ESP32: PSRAM size: 8388608 bytes
I (XXX) TEN_VAD_ESP32: PSRAM is available and enabled
I (XXX) TEN_VAD_ESP32: ===================================
I (XXX) TEN_VAD_ESP32: Starting TEN VAD Test on ESP32-S3
I (XXX) TEN_VAD_ESP32: TEN VAD version: 1.0
I (XXX) TEN_VAD_ESP32: Frame[0] probability: 0.XXXXXX, voice detected: YES/NO
...
I (XXX) TEN_VAD_ESP32: === Performance Results ===
I (XXX) TEN_VAD_ESP32: Processing time: XX.XX ms
I (XXX) TEN_VAD_ESP32: Audio duration: XX.XX ms
I (XXX) TEN_VAD_ESP32: RTF (Real-Time Factor): 0.XXXXXX
I (XXX) TEN_VAD_ESP32: =========================
```

## Memory Usage

The TEN VAD library is optimized for ESP32-S3 with efficient memory usage:

- **Code size**: ~300KB
- **RAM usage**: Minimal internal RAM, uses PSRAM for buffers
- **PSRAM usage**: Dynamic allocation for audio processing buffers

The implementation automatically uses PSRAM for large allocations, keeping internal RAM free for critical operations.

## Project Structure

```
examples_esp32/
├── CMakeLists.txt              # Main ESP-IDF project file
├── sdkconfig.defaults          # Default ESP32-S3 configuration
├── README.md                   # This file
├── main/
│   ├── CMakeLists.txt         # Main component build config
│   └── main.c                 # Application entry point
└── components/
    └── ten_vad/
        └── CMakeLists.txt     # TEN VAD component build config
```

## Customization

### Using Real Audio Input

The current implementation uses hardcoded test audio samples. To use real audio input:

1. Add I2S driver support for your audio input device
2. Configure I2S in `main.c`
3. Replace the test samples with real-time audio frames from I2S
4. Process audio in real-time using the VAD

Example I2S configuration (to be added):

```c
i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_RX,
    .sample_rate = 16000,  // 16kHz for TEN VAD
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,  // Match hop_size
    .use_apll = false,
};
```

### Adjusting VAD Parameters

In `main.c`, you can adjust:

- `hop_size`: Frame size (default: 256 samples = 16ms at 16kHz)
- `voice_threshold`: Detection threshold (default: 0.5, range: 0.0-1.0)

```c
const int hop_size = 256; // or 160 for 10ms frames
float voice_threshold = 0.5f; // adjust based on your use case
```

## Performance

TEN VAD on ESP32-S3 achieves:

- **Real-Time Factor (RTF)**: < 0.1 (typically)
- **Latency**: ~16ms per frame
- **CPU Usage**: Low, leaving plenty of resources for other tasks
- **Memory Efficiency**: Uses PSRAM for buffers, minimal internal RAM

## Troubleshooting

### Build Errors

1. **ESP-IDF not found**: Make sure you've run `. $HOME/esp/esp-idf/export.sh`
2. **CMake errors**: Ensure CMake version is 3.16 or later
3. **C++ compilation errors**: Make sure C++ exceptions and RTTI are enabled

### Runtime Errors

1. **PSRAM not detected**: Check your board has PSRAM and it's properly configured
2. **Memory allocation failures**: Increase PSRAM malloc reserve size in menuconfig
3. **Crashes during VAD processing**: Increase main task stack size in sdkconfig.defaults

### Flash Errors

1. **Serial port not found**: Check USB connection and port name
2. **Permission denied**: Add user to dialout group (Linux) or check COM port permissions
3. **Boot loop**: Check power supply is adequate (ESP32-S3 with PSRAM needs stable power)

## Integration with Your Project

To integrate TEN VAD into your ESP-IDF project:

1. Copy the `components/ten_vad` directory to your project's `components/` directory
2. Add `REQUIRES ten_vad` to your main component's `idf_component_register()` call
3. Include `ten_vad.h` in your code
4. Call the TEN VAD API functions as shown in `main.c`

## API Reference

See `include/ten_vad.h` in the root directory for the complete API documentation.

### Basic Usage

```c
#include "ten_vad.h"

// Create VAD instance
void *vad_handle = NULL;
ten_vad_create(&vad_handle, 256, 0.5f);

// Process audio frame (256 samples at 16kHz = 16ms)
int16_t audio_frame[256];
float probability;
int voice_detected;

ten_vad_process(vad_handle, audio_frame, 256, &probability, &voice_detected);

// Clean up
ten_vad_destroy(&vad_handle);
```

## License

This project is licensed pursuant to the Apache 2.0 with additional conditions. Refer to the "LICENSE" file in the root directory for detailed information.

## Support

For questions and support:
- GitHub Issues: https://github.com/TEN-framework/ten-vad/issues
- Discord: https://discord.gg/VnPftUzAMJ
- Documentation: See main README.md in the repository root

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.
