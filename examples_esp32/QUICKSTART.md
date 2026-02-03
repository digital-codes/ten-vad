# TEN VAD ESP32-S3 Quick Start Guide

Get TEN VAD running on ESP32-S3 in under 10 minutes!

## Prerequisites

- ESP32-S3 development board with 8MB PSRAM
- USB cable
- Linux, macOS, or Windows with WSL

## Step 1: Install ESP-IDF v5 (5 minutes)

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install git wget flex bison gperf python3 python3-pip \
    python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
    dfu-util libusb-1.0-0

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install ESP-IDF tools for ESP32-S3
./install.sh esp32s3

# Setup environment (add this to your ~/.bashrc for permanent setup)
. ./export.sh
```

## Step 2: Clone and Build TEN VAD (3 minutes)

```bash
# Clone repository
cd ~
git clone https://github.com/TEN-framework/ten-vad.git
cd ten-vad/examples_esp32

# Set target and build
idf.py set-target esp32s3
idf.py build
```

## Step 3: Flash and Run (2 minutes)

```bash
# Connect your ESP32-S3 board via USB
# Find your serial port (usually /dev/ttyUSB0 or /dev/ttyACM0)
ls /dev/tty*

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Press Ctrl+] to exit monitor
```

## Expected Output

```
I (XXX) TEN_VAD_ESP32: ===================================
I (XXX) TEN_VAD_ESP32: TEN VAD ESP32-S3 Demo Application
I (XXX) TEN_VAD_ESP32: ===================================
I (XXX) TEN_VAD_ESP32: PSRAM size: 8388608 bytes
I (XXX) TEN_VAD_ESP32: Starting TEN VAD Test on ESP32-S3
I (XXX) TEN_VAD_ESP32: TEN VAD version: 1.0
I (XXX) TEN_VAD_ESP32: Frame[0] probability: 0.XXXXXX, voice detected: YES/NO
...
I (XXX) TEN_VAD_ESP32: === Performance Results ===
I (XXX) TEN_VAD_ESP32: RTF (Real-Time Factor): 0.XXXXXX
```

## Troubleshooting

**Build fails with "ESP-IDF not found":**
- Run `. ~/esp/esp-idf/export.sh` in your terminal

**Serial port not found:**
- Check USB connection
- Try different USB port
- Add user to dialout group: `sudo usermod -a -G dialout $USER`

**PSRAM not detected:**
- Verify your board has PSRAM (check specifications)
- Try power cycling the board

## Next Steps

- Try the I2S microphone example (see README.md)
- Integrate VAD into your project
- Adjust threshold for your use case

## Support

- Documentation: See main README.md
- Issues: https://github.com/TEN-framework/ten-vad/issues
- Discord: https://discord.gg/VnPftUzAMJ
