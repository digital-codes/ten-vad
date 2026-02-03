//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// ESP32-S3 Advanced Example with I2S Microphone Support
//
// This example shows how to integrate TEN VAD with I2S audio input
// from a microphone (e.g., INMP441, SPH0645, ICS-43434)
//
// Features:
// - 8kHz sample rate for optimized performance
// - LED indicator for voice detection
// - Circular buffer for audio recording (up to 15 seconds)
// - Parallel recording and transmission tasks
// - Hook for remote server transmission (to be implemented)
//
// To use this example:
// 1. Rename this file to main.c (replace the simple test version)
// 2. Connect your I2S microphone to ESP32-S3:
//    - WS (LRCK) -> GPIO 15
//    - SCK (BCLK) -> GPIO 14  
//    - SD (DATA) -> GPIO 13
// 3. Connect LED to GPIO 2 (built-in LED on most boards)
// 4. Build and flash: idf.py build flash monitor

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "ten_vad.h"

static const char *TAG = "TEN_VAD_I2S";

// I2S Configuration - Optimized for 8kHz
#define I2S_NUM             I2S_NUM_0
#define I2S_SAMPLE_RATE     8000   // 8kHz for optimized performance
#define I2S_BCLK_GPIO       GPIO_NUM_14
#define I2S_WS_GPIO         GPIO_NUM_15
#define I2S_DIN_GPIO        GPIO_NUM_13
#define I2S_CHANNEL_NUM     1      // Mono

// LED Configuration
#define LED_GPIO            GPIO_NUM_2  // Built-in LED on most ESP32-S3 boards

// VAD Configuration - Adjusted for 8kHz
#define VAD_HOP_SIZE        128    // 16ms at 8kHz (128 samples = 16ms)
#define VAD_THRESHOLD       0.5f   // Adjust based on your environment

// Audio buffer configuration
#define DMA_BUF_COUNT       8
#define DMA_BUF_LEN         VAD_HOP_SIZE

// Recording buffer configuration
#define MAX_RECORDING_DURATION_SEC  15  // Maximum recording duration in seconds
#define RECORDING_BUFFER_SIZE       (I2S_SAMPLE_RATE * MAX_RECORDING_DURATION_SEC)  // 15 seconds at 8kHz
#define RECORDING_CHUNK_SIZE        VAD_HOP_SIZE  // Record in chunks matching VAD frame size

// Transmission configuration
#define TRANSMISSION_CHUNK_DURATION_SEC  2      // Seconds of audio per transmission
#define TRANSMISSION_CHUNK_SIZE          (I2S_SAMPLE_RATE * TRANSMISSION_CHUNK_DURATION_SEC)  // Samples per transmission
#define TRANSMISSION_CHECK_INTERVAL_MS   2000   // Check buffer every 2 seconds
#define STATS_REPORT_INTERVAL_US         5000000  // Report statistics every 5 seconds (microseconds)

// Circular buffer for recording
typedef struct {
    int16_t *buffer;
    size_t size;
    size_t write_index;
    size_t read_index;
    size_t count;
    SemaphoreHandle_t mutex;
    bool is_recording;
} circular_buffer_t;

static i2s_chan_handle_t rx_handle = NULL;
static void *vad_handle = NULL;
static circular_buffer_t *record_buffer = NULL;

// Note: voice_activity_detected is volatile for visibility across tasks
// It is written only by VAD task and can be read by other tasks
// For simple boolean flag updates, volatile is sufficient without mutex
static volatile bool voice_activity_detected = false;

/**
 * Initialize LED GPIO
 */
esp_err_t init_led(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Turn off LED initially
    gpio_set_level(LED_GPIO, 0);
    ESP_LOGI(TAG, "LED initialized on GPIO %d", LED_GPIO);
    return ESP_OK;
}

/**
 * Set LED state based on voice detection
 */
void set_led_state(bool voice_detected)
{
    gpio_set_level(LED_GPIO, voice_detected ? 1 : 0);
}

/**
 * Create circular buffer for recording
 */
circular_buffer_t* create_circular_buffer(size_t size)
{
    circular_buffer_t *cb = (circular_buffer_t *)heap_caps_malloc(sizeof(circular_buffer_t), 
                                                                    MALLOC_CAP_8BIT);
    if (cb == NULL) {
        ESP_LOGE(TAG, "Failed to allocate circular buffer structure");
        return NULL;
    }
    
    cb->buffer = (int16_t *)heap_caps_malloc(size * sizeof(int16_t), 
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (cb->buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate circular buffer memory");
        heap_caps_free(cb);
        return NULL;
    }
    
    cb->mutex = xSemaphoreCreateMutex();
    if (cb->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        heap_caps_free(cb->buffer);
        heap_caps_free(cb);
        return NULL;
    }
    
    cb->size = size;
    cb->write_index = 0;
    cb->read_index = 0;
    cb->count = 0;
    cb->is_recording = false;
    
    ESP_LOGI(TAG, "Circular buffer created: %d samples (%.1f seconds at %d Hz)",
             (int)size, (float)size / I2S_SAMPLE_RATE, I2S_SAMPLE_RATE);
    
    return cb;
}

/**
 * Destroy circular buffer
 */
void destroy_circular_buffer(circular_buffer_t *cb)
{
    if (cb != NULL) {
        if (cb->mutex != NULL) {
            vSemaphoreDelete(cb->mutex);
        }
        if (cb->buffer != NULL) {
            heap_caps_free(cb->buffer);
        }
        heap_caps_free(cb);
    }
}

/**
 * Write samples to circular buffer
 * Returns number of samples written
 */
size_t circular_buffer_write(circular_buffer_t *cb, const int16_t *data, size_t samples)
{
    if (cb == NULL || data == NULL || samples == 0) {
        return 0;
    }
    
    if (xSemaphoreTake(cb->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for write operation");
        return 0;
    }
    
    size_t written = 0;
    
    for (size_t i = 0; i < samples; i++) {
        cb->buffer[cb->write_index] = data[i];
        cb->write_index = (cb->write_index + 1) % cb->size;
        
        if (cb->count < cb->size) {
            cb->count++;
        } else {
            // Buffer is full, overwrite oldest data
            cb->read_index = (cb->read_index + 1) % cb->size;
        }
        written++;
    }
    
    xSemaphoreGive(cb->mutex);
    return written;
}

/**
 * Read samples from circular buffer
 * Returns number of samples read
 */
size_t circular_buffer_read(circular_buffer_t *cb, int16_t *data, size_t samples)
{
    if (cb == NULL || data == NULL || samples == 0) {
        return 0;
    }
    
    if (xSemaphoreTake(cb->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for read operation");
        return 0;
    }
    
    size_t to_read = (samples < cb->count) ? samples : cb->count;
    size_t read = 0;
    
    for (size_t i = 0; i < to_read; i++) {
        data[i] = cb->buffer[cb->read_index];
        cb->read_index = (cb->read_index + 1) % cb->size;
        cb->count--;
        read++;
    }
    
    xSemaphoreGive(cb->mutex);
    return read;
}

/**
 * Get number of available samples in buffer
 */
size_t circular_buffer_available(circular_buffer_t *cb)
{
    if (cb == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(cb->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for available check");
        return 0;
    }
    
    size_t count = cb->count;
    xSemaphoreGive(cb->mutex);
    
    return count;
}

/**
 * Transmission hook for sending recorded audio to remote server
 * This is a stub that will be implemented later
 * 
 * @param audio_data Pointer to audio samples (int16_t PCM)
 * @param sample_count Number of samples in the buffer
 * @param transmission_context User-defined context for transmission (e.g., server config)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t transmit_audio_hook(const int16_t *audio_data, size_t sample_count, void *transmission_context)
{
    // TODO: Implement actual transmission to remote server
    // This could use HTTP, MQTT, WebSocket, or other protocols
    
    ESP_LOGD(TAG, "Transmission hook called with %d samples", (int)sample_count);
    
    // Example: You would typically do something like:
    // - Format the audio data (e.g., encode to Opus, MP3, or send raw PCM)
    // - Send via HTTP POST, MQTT publish, WebSocket send, etc.
    // - Handle acknowledgments and retries
    
    // For now, just return success
    return ESP_OK;
}

/**
 * Transmission task - runs in parallel with recording
 * Periodically checks the buffer and transmits recorded audio
 */
void transmission_task(void *pvParameters)
{
    circular_buffer_t *cb = (circular_buffer_t *)pvParameters;
    int16_t *transmission_buffer = NULL;
    
    transmission_buffer = (int16_t *)heap_caps_malloc(TRANSMISSION_CHUNK_SIZE * sizeof(int16_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (transmission_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate transmission buffer");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Transmission task started");
    
    while (1) {
        // Wait for some data to accumulate in the buffer
        vTaskDelay(pdMS_TO_TICKS(TRANSMISSION_CHECK_INTERVAL_MS));
        
        size_t available = circular_buffer_available(cb);
        
        if (available >= TRANSMISSION_CHUNK_SIZE) {
            // Read audio data from buffer
            size_t read = circular_buffer_read(cb, transmission_buffer, TRANSMISSION_CHUNK_SIZE);
            
            if (read > 0) {
                ESP_LOGI(TAG, "Transmitting %d samples to remote server...", (int)read);
                
                // Call the transmission hook
                esp_err_t ret = transmit_audio_hook(transmission_buffer, read, NULL);
                
                if (ret == ESP_OK) {
                    ESP_LOGD(TAG, "Transmission successful");
                } else {
                    ESP_LOGW(TAG, "Transmission failed: %s", esp_err_to_name(ret));
                }
            }
        }
    }
    
    heap_caps_free(transmission_buffer);
    vTaskDelete(NULL);
}

/**
 * Initialize I2S for microphone input
 */
esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws = I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2s_channel_enable(rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2S initialized successfully");
    ESP_LOGI(TAG, "Sample rate: %d Hz (optimized for 8kHz), Bits: 16, Channels: %d", I2S_SAMPLE_RATE, I2S_CHANNEL_NUM);
    return ESP_OK;
}

/**
 * Initialize TEN VAD
 */
esp_err_t init_vad(void)
{
    int ret = ten_vad_create(&vad_handle, VAD_HOP_SIZE, VAD_THRESHOLD);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create VAD instance: %d", ret);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "TEN VAD initialized successfully");
    ESP_LOGI(TAG, "Version: %s", ten_vad_get_version());
    ESP_LOGI(TAG, "Hop size: %d samples (%.1f ms)", VAD_HOP_SIZE, (float)VAD_HOP_SIZE / I2S_SAMPLE_RATE * 1000);
    ESP_LOGI(TAG, "Threshold: %.2f", VAD_THRESHOLD);
    return ESP_OK;
}

/**
 * Main VAD processing task with recording support
 */
void vad_task(void *pvParameters)
{
    int16_t *audio_buffer = (int16_t *)heap_caps_malloc(VAD_HOP_SIZE * sizeof(int16_t), 
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }
    
    float probability;
    int voice_detected;
    size_t bytes_read;
    uint32_t frame_count = 0;
    uint32_t voice_frames = 0;
    uint64_t last_report_time = esp_timer_get_time();
    uint64_t recording_start_time = 0;
    bool is_recording = false;
    
    ESP_LOGI(TAG, "Starting continuous VAD processing with recording...");
    ESP_LOGI(TAG, "Sample rate: %d Hz, Frame size: %d samples (%.1f ms)", 
             I2S_SAMPLE_RATE, VAD_HOP_SIZE, (float)VAD_HOP_SIZE / I2S_SAMPLE_RATE * 1000);
    ESP_LOGI(TAG, "Max recording duration: %d seconds", MAX_RECORDING_DURATION_SEC);
    ESP_LOGI(TAG, "Press Ctrl+] to exit monitor");
    
    while (1) {
        // Read audio data from I2S
        esp_err_t i2s_ret = i2s_channel_read(rx_handle, audio_buffer, 
                                         VAD_HOP_SIZE * sizeof(int16_t), 
                                         &bytes_read, portMAX_DELAY);
        
        if (i2s_ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(i2s_ret));
            continue;
        }
        
        if (bytes_read != VAD_HOP_SIZE * sizeof(int16_t)) {
            ESP_LOGW(TAG, "Incomplete frame: %d bytes", bytes_read);
            continue;
        }
        
        // Process with VAD
        int vad_ret = ten_vad_process(vad_handle, audio_buffer, VAD_HOP_SIZE,
                             &probability, &voice_detected);
        
        if (vad_ret == 0) {
            frame_count++;
            
            // Update voice activity flag and LED
            voice_activity_detected = voice_detected;
            set_led_state(voice_detected);
            
            if (voice_detected) {
                voice_frames++;
                
                // Start recording on voice detection
                if (!is_recording) {
                    is_recording = true;
                    recording_start_time = esp_timer_get_time();
                    ESP_LOGI(TAG, "🎤 VOICE DETECTED - RECORDING STARTED [%u] p=%.3f", frame_count, probability);
                } else {
                    ESP_LOGD(TAG, "🎤 VOICE [%u] p=%.3f", frame_count, probability);
                }
                
                // Write to recording buffer
                circular_buffer_write(record_buffer, audio_buffer, VAD_HOP_SIZE);
                
            } else {
                // Check if recording should stop (after max duration or inactivity)
                if (is_recording) {
                    uint64_t recording_duration_us = esp_timer_get_time() - recording_start_time;
                    float recording_duration_sec = recording_duration_us / 1000000.0f;
                    
                    // Stop recording after max duration
                    if (recording_duration_sec >= MAX_RECORDING_DURATION_SEC) {
                        is_recording = false;
                        ESP_LOGI(TAG, "Recording stopped (max duration reached: %.1f seconds)", 
                                recording_duration_sec);
                        ESP_LOGI(TAG, "Recorded samples available: %d (%.1f seconds)",
                                (int)circular_buffer_available(record_buffer),
                                circular_buffer_available(record_buffer) / (float)I2S_SAMPLE_RATE);
                    } else {
                        // Continue recording for a bit even during silence
                        // This provides context around speech
                        circular_buffer_write(record_buffer, audio_buffer, VAD_HOP_SIZE);
                    }
                }
            }
            
            // Report statistics every 5 seconds
            uint64_t current_time = esp_timer_get_time();
            if (current_time - last_report_time >= STATS_REPORT_INTERVAL_US) {
                float voice_percentage = (float)voice_frames / frame_count * 100;
                size_t buffer_available = circular_buffer_available(record_buffer);
                float buffer_duration = buffer_available / (float)I2S_SAMPLE_RATE;
                
                ESP_LOGI(TAG, "=== Statistics (last 5s) ===");
                ESP_LOGI(TAG, "Total frames: %u", frame_count);
                ESP_LOGI(TAG, "Voice frames: %u (%.1f%%)", voice_frames, voice_percentage);
                ESP_LOGI(TAG, "Recording buffer: %d samples (%.1f seconds)", 
                        (int)buffer_available, buffer_duration);
                ESP_LOGI(TAG, "Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
                ESP_LOGI(TAG, "============================");
                
                // Reset counters
                frame_count = 0;
                voice_frames = 0;
                last_report_time = current_time;
            }
        } else {
            ESP_LOGE(TAG, "VAD processing failed: %d", vad_ret);
        }
        
        // Small delay to prevent watchdog timeout
        vTaskDelay(1);
    }
    
    heap_caps_free(audio_buffer);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, "TEN VAD ESP32-S3 with I2S Microphone");
    ESP_LOGI(TAG, "Optimized Version: 8kHz, LED, Recording");
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    
    // Check PSRAM
    if (esp_psram_get_size() > 0) {
        ESP_LOGI(TAG, "PSRAM: %d bytes", esp_psram_get_size());
    } else {
        ESP_LOGW(TAG, "PSRAM not available!");
    }
    
    ESP_LOGI(TAG, "===============================================");
    
    // Initialize LED
    if (init_led() != ESP_OK) {
        ESP_LOGE(TAG, "LED initialization failed!");
        return;
    }
    
    // Initialize I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "I2S initialization failed!");
        return;
    }
    
    // Initialize VAD
    if (init_vad() != ESP_OK) {
        ESP_LOGE(TAG, "VAD initialization failed!");
        return;
    }
    
    // Create recording buffer
    record_buffer = create_circular_buffer(RECORDING_BUFFER_SIZE);
    if (record_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to create recording buffer!");
        return;
    }
    
    // Print memory info
    ESP_LOGI(TAG, "Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    // Create VAD processing task (handles recording)
    xTaskCreatePinnedToCore(
        vad_task,           // Task function
        "vad_task",         // Task name
        8192,               // Stack size
        NULL,               // Parameters
        5,                  // Priority (higher for real-time audio processing)
        NULL,               // Task handle
        1                   // Core ID (1 for APP CPU)
    );
    
    // Create transmission task (runs in parallel)
    xTaskCreatePinnedToCore(
        transmission_task,  // Task function
        "tx_task",          // Task name
        4096,               // Stack size
        record_buffer,      // Pass buffer as parameter
        3,                  // Priority (lower than VAD task)
        NULL,               // Task handle
        0                   // Core ID (0 for PRO CPU, separate from VAD)
    );
    
    ESP_LOGI(TAG, "System running. VAD is active with recording and transmission.");
    ESP_LOGI(TAG, "LED on GPIO %d indicates voice detection status.", LED_GPIO);
}
