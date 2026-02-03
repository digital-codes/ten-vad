#!/bin/bash
# Build script for TEN VAD ESP32-S3

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if ESP-IDF is installed
if ! command -v idf.py &> /dev/null; then
    echo -e "${RED}Error: ESP-IDF is not installed or not in PATH${NC}"
    echo "Please install ESP-IDF v5.x and run the export script:"
    echo "  . \$HOME/esp/esp-idf/export.sh"
    echo ""
    echo "For installation instructions, visit:"
    echo "  https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/"
    exit 1
fi

echo -e "${GREEN}ESP-IDF found${NC}"
idf.py --version

# Check if target is set
if [ ! -f "sdkconfig" ]; then
    echo -e "${YELLOW}Setting target to esp32s3...${NC}"
    idf.py set-target esp32s3
fi

# Build the project
echo -e "${GREEN}Building TEN VAD for ESP32-S3...${NC}"
idf.py build

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo ""
    echo "To flash to your ESP32-S3 board:"
    echo "  idf.py -p /dev/ttyUSB0 flash monitor"
    echo ""
    echo "Replace /dev/ttyUSB0 with your actual serial port"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
