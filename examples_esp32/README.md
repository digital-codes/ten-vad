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
├── CMakeLists.txt                  # Main ESP-IDF project file
├── sdkconfig.defaults              # Default ESP32-S3 configuration
├── build.sh                        # Build helper script
├── README.md                       # This file
├── main/
│   ├── CMakeLists.txt             # Main component build config
│   ├── main.c                     # Simple test application
│   └── main_i2s_example.c         # Advanced I2S microphone example
└── components/
    └── ten_vad/
        └── CMakeLists.txt         # TEN VAD component build config
```

## Customization

### Using Real Audio Input (I2S Microphone)

An advanced example with I2S microphone support is provided in `main/main_i2s_example.c`.

To use it:

1. Connect your I2S microphone (e.g., INMP441, SPH0645, ICS-43434):
   - WS (LRCK) → GPIO 15
   - SCK (BCLK) → GPIO 14
   - SD (DATA) → GPIO 13
   - VDD → 3.3V
   - GND → GND

2. Connect an LED to GPIO 2 (or use the built-in LED on most ESP32-S3 boards):
   - LED anode → GPIO 2
   - LED cathode → GND (through appropriate resistor, e.g., 220Ω)

3. Replace `main.c` with the I2S example:
   ```bash
   cd main
   mv main.c main_simple.c
   mv main_i2s_example.c main.c
   cd ..
   ```

4. Rebuild and flash:
   ```bash
   idf.py build flash monitor
   ```

The I2S example provides:
- **Optimized 8kHz sample rate** for better performance
- **LED indicator** on GPIO 2 that lights up when voice is detected
- **Continuous real-time VAD processing** with direct microphone input
- **Audio recording buffer** (up to 15 seconds)
- **Parallel recording and transmission tasks** running on separate CPU cores
- **Circular buffer** with mutex synchronization for thread-safe operation
- **Transmission hook** for sending recorded audio to remote server (stub for future implementation)
- **Voice activity statistics** with detailed reporting
- **Production-ready structure** with proper resource management

### Advanced Features

#### Audio Recording
The I2S example includes a circular buffer that can store up to 15 seconds of audio at 8kHz. Recording automatically starts when voice is detected and continues until the maximum duration is reached. The buffer uses PSRAM for efficient memory usage.

#### Parallel Processing
The implementation uses FreeRTOS to run recording and transmission in parallel:
- **VAD Task** (Core 1): Handles I2S audio input, VAD processing, and writing to the recording buffer
- **Transmission Task** (Core 0): Reads from the recording buffer and sends data to a remote server

Both tasks are synchronized using mutexes to ensure thread-safe access to the shared circular buffer.

#### Transmission Hook
A transmission hook function `transmit_audio_hook()` is provided as a stub for future implementation. You can customize this to:
- Send audio via HTTP POST to a REST API
- Publish audio data via MQTT
- Stream audio via WebSocket
- Upload to cloud storage
- Use any other protocol suitable for your application

### Adjusting VAD Parameters

In `main_i2s_example.c`, you can adjust:

- `I2S_SAMPLE_RATE`: Audio sample rate (default: 8000 Hz for optimized performance)
- `VAD_HOP_SIZE`: Frame size (default: 128 samples = 16ms at 8kHz)
- `VAD_THRESHOLD`: Detection threshold (default: 0.5, range: 0.0-1.0)
- `MAX_RECORDING_DURATION_SEC`: Maximum recording duration (default: 15 seconds)
- `LED_GPIO`: GPIO pin for LED indicator (default: GPIO 2)

```c
// I2S Configuration - can be changed to 16kHz if needed
#define I2S_SAMPLE_RATE     8000   // 8kHz for optimized performance

// VAD Configuration - adjusted for 8kHz
#define VAD_HOP_SIZE        128    // 16ms at 8kHz (or 256 for 16kHz)
#define VAD_THRESHOLD       0.5f   // adjust based on your use case

// Recording Configuration
#define MAX_RECORDING_DURATION_SEC  15  // Maximum recording duration

// LED Configuration
#define LED_GPIO            GPIO_NUM_2  // Change to your preferred GPIO
```

**Note on Sample Rates:**
- **8kHz**: Optimized for voice, lower CPU usage, smaller buffer sizes (recommended)
- **16kHz**: Standard VAD rate, higher quality, more CPU usage

When changing the sample rate, adjust `VAD_HOP_SIZE` accordingly:
- For 8kHz: Use 128 samples for 16ms frames
- For 16kHz: Use 256 samples for 16ms frames

## Performance

TEN VAD on ESP32-S3 achieves:

- **Real-Time Factor (RTF)**: < 0.1 (typically)
- **Latency**: ~16ms per frame
- **CPU Usage**: Low, leaving plenty of resources for other tasks
- **Memory Efficiency**: Uses PSRAM for buffers, minimal internal RAM
- **Dual-Core Processing**: VAD and transmission tasks run in parallel on separate cores
- **Recording Buffer**: 15 seconds at 8kHz (120KB in PSRAM)
- **Sample Rate Optimization**: 8kHz reduces bandwidth and processing requirements by 50% compared to 16kHz

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
