# ESP32-S3 Port - Integration Testing Guide

This document provides testing instructions for the ESP32-S3 port of TEN VAD.

## Test Plan

### 1. Build Test
**Objective**: Verify that the project builds successfully with ESP-IDF v5

**Prerequisites**:
- ESP-IDF v5.x installed and configured
- Build tools (CMake, Ninja, etc.)

**Steps**:
```bash
cd examples_esp32
idf.py set-target esp32s3
idf.py build
```

**Expected Result**:
- Build completes without errors
- Binary files generated in `build/` directory
- Firmware size within ESP32-S3 limits (~300KB for app)

**Success Criteria**:
✅ Build completes successfully
✅ No compiler warnings about missing headers
✅ No linker errors
✅ Build output shows correct target (ESP32-S3)

---

### 2. Flash and Boot Test
**Objective**: Verify firmware can be flashed and boots correctly

**Prerequisites**:
- ESP32-S3 board with 8MB PSRAM
- USB connection

**Steps**:
```bash
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

**Expected Output**:
```
I (XXX) TEN_VAD_ESP32: ===================================
I (XXX) TEN_VAD_ESP32: TEN VAD ESP32-S3 Demo Application
I (XXX) TEN_VAD_ESP32: ===================================
I (XXX) TEN_VAD_ESP32: ESP-IDF Version: v5.x.x
I (XXX) TEN_VAD_ESP32: Free heap at startup: XXXXX bytes
I (XXX) TEN_VAD_ESP32: PSRAM size: 8388608 bytes
I (XXX) TEN_VAD_ESP32: PSRAM is available and enabled
```

**Success Criteria**:
✅ Firmware flashes successfully
✅ Board boots and runs application
✅ PSRAM is detected (8388608 bytes)
✅ No boot errors or crashes

---

### 3. Basic VAD Functionality Test
**Objective**: Verify VAD processing works correctly

**Prerequisites**:
- Test firmware running on ESP32-S3

**Expected Output**:
```
I (XXX) TEN_VAD_ESP32: Starting TEN VAD Test on ESP32-S3
I (XXX) TEN_VAD_ESP32: TEN VAD version: 1.0
I (XXX) TEN_VAD_ESP32: Sample count: XXX
I (XXX) TEN_VAD_ESP32: Frame count: X
I (XXX) TEN_VAD_ESP32: Frame[0] probability: 0.XXXXXX, voice detected: YES/NO
...
I (XXX) TEN_VAD_ESP32: === Performance Results ===
I (XXX) TEN_VAD_ESP32: Processing time: XX.XX ms
I (XXX) TEN_VAD_ESP32: RTF (Real-Time Factor): 0.XXXXXX
I (XXX) TEN_VAD_ESP32: VAD test completed successfully
```

**Success Criteria**:
✅ VAD instance created successfully
✅ All frames processed without errors
✅ Probability values in range [0.0, 1.0]
✅ Voice detection flags (0 or 1) returned
✅ RTF < 0.5 (real-time capable)
✅ No crashes or memory errors

---

### 4. Memory Usage Test
**Objective**: Verify efficient memory usage and PSRAM allocation

**Check Points**:
- Initial free heap (should be > 200KB internal)
- PSRAM usage (buffers should use PSRAM)
- No memory leaks during processing
- Sufficient memory after VAD initialization

**Expected Values**:
- Free internal RAM: > 200KB after boot
- Free PSRAM: > 7MB after initialization
- PSRAM used for large allocations (audio buffers)

**Success Criteria**:
✅ PSRAM properly utilized
✅ Internal RAM preserved for critical operations
✅ No memory leaks detected
✅ Stable memory usage during operation

---

### 5. Performance Test
**Objective**: Verify real-time performance

**Metrics**:
- RTF (Real-Time Factor): Should be << 1.0
- Processing latency: ~16ms per frame
- CPU usage: Should leave headroom for other tasks

**Success Criteria**:
✅ RTF < 0.1 (typically)
✅ Consistent processing time
✅ No frame drops or delays
✅ System remains responsive

---

### 6. I2S Integration Test (Advanced)
**Objective**: Verify I2S microphone integration works

**Prerequisites**:
- I2S microphone (INMP441, SPH0645, etc.)
- Proper wiring to ESP32-S3
- `main_i2s_example.c` compiled and flashed

**Steps**:
1. Wire I2S microphone to ESP32-S3:
   - WS → GPIO 15
   - SCK → GPIO 14
   - SD → GPIO 13
   - VDD → 3.3V
   - GND → GND

2. Flash I2S example:
   ```bash
   cd main
   mv main.c main_simple.c
   mv main_i2s_example.c main.c
   cd ..
   idf.py build flash monitor
   ```

3. Speak near microphone and observe output

**Expected Output**:
```
I (XXX) TEN_VAD_I2S: I2S initialized successfully
I (XXX) TEN_VAD_I2S: Starting continuous VAD processing...
I (XXX) TEN_VAD_I2S: 🎤 VOICE [X] p=0.XXX  (when speaking)
I (XXX) TEN_VAD_I2S: === Statistics (last 5s) ===
I (XXX) TEN_VAD_I2S: Voice frames: XX (XX.X%)
```

**Success Criteria**:
✅ I2S initializes successfully
✅ Audio data captured correctly
✅ Voice detected when speaking
✅ Silence detected when quiet
✅ Stable continuous operation
✅ Statistics show reasonable voice percentages

---

### 7. Stress Test
**Objective**: Verify system stability under continuous operation

**Steps**:
1. Run application for extended period (30+ minutes)
2. Monitor for crashes, memory leaks, or performance degradation

**Success Criteria**:
✅ No crashes or resets
✅ Memory usage remains stable
✅ Performance remains consistent
✅ No watchdog timeouts

---

### 8. Integration Test
**Objective**: Verify easy integration into user projects

**Steps**:
1. Copy `components/ten_vad` to a new ESP-IDF project
2. Add `REQUIRES ten_vad` to main component
3. Include and use TEN VAD API

**Success Criteria**:
✅ Component integrates cleanly
✅ No build errors
✅ API works as documented

---

## Test Environment

### Hardware Requirements
- ESP32-S3-DevKitC-1 (or similar)
- 8MB PSRAM (verified in specifications)
- USB cable (data capable)
- Optional: I2S microphone for advanced testing

### Software Requirements
- ESP-IDF v5.0 or later (v5.3 recommended)
- Python 3.8+
- USB-to-serial drivers

### Recommended Test Boards
- ESP32-S3-DevKitC-1-N8R8 (8MB PSRAM)
- ESP32-S3-WROOM-1-N8R8 (8MB PSRAM)
- ESP32-S3-DevKitM-1-N8 (8MB PSRAM)

---

## Known Limitations

1. **Audio Input**: Basic example uses hardcoded samples. For real audio, use I2S example.
2. **Sample Rate**: Currently fixed at 16kHz (TEN VAD requirement)
3. **Hop Size**: 256 or 160 samples supported
4. **Platform**: Currently tested only with ESP-IDF v5.x

---

## Reporting Issues

When reporting issues, please include:
- ESP-IDF version (`idf.py --version`)
- Board model and PSRAM size
- Full serial output (from boot to error)
- Build configuration (`sdkconfig`)
- Steps to reproduce

Report at: https://github.com/TEN-framework/ten-vad/issues

---

## Success Checklist

Use this checklist to verify the port is working correctly:

- [ ] Build completes successfully
- [ ] Firmware flashes to board
- [ ] Board boots without errors
- [ ] PSRAM detected (8MB)
- [ ] VAD initializes successfully
- [ ] Test frames process correctly
- [ ] RTF < 0.1 (real-time capable)
- [ ] Memory usage acceptable
- [ ] No crashes during operation
- [ ] I2S example works (if tested)
- [ ] Documentation is clear and accurate

---

## Validation Signature

**Tested By**: _______________  
**Date**: _______________  
**Board Model**: _______________  
**ESP-IDF Version**: _______________  
**Result**: ☐ PASS  ☐ FAIL  
**Notes**: _______________________________________________

