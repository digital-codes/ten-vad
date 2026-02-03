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
// To use this example:
// 1. Rename this file to main.c (replace the simple test version)
// 2. Connect your I2S microphone to ESP32-S3:
//    - WS (LRCK) -> GPIO 15
//    - SCK (BCLK) -> GPIO 14  
//    - SD (DATA) -> GPIO 13
// 3. Build and flash: idf.py build flash monitor

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "ten_vad.h"

static const char *TAG = "TEN_VAD_I2S";

// I2S Configuration
#define I2S_NUM             I2S_NUM_0
#define I2S_SAMPLE_RATE     16000  // 16kHz for TEN VAD
#define I2S_BCLK_GPIO       GPIO_NUM_14
#define I2S_WS_GPIO         GPIO_NUM_15
#define I2S_DIN_GPIO        GPIO_NUM_13
#define I2S_CHANNEL_NUM     1      // Mono

// VAD Configuration
#define VAD_HOP_SIZE        256    // 16ms at 16kHz
#define VAD_THRESHOLD       0.5f   // Adjust based on your environment

// Audio buffer configuration
#define DMA_BUF_COUNT       8
#define DMA_BUF_LEN         VAD_HOP_SIZE

static i2s_chan_handle_t rx_handle = NULL;
static void *vad_handle = NULL;

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
    ESP_LOGI(TAG, "Sample rate: %d Hz, Bits: 16, Channels: %d", I2S_SAMPLE_RATE, I2S_CHANNEL_NUM);
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
 * Main VAD processing task
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
    
    ESP_LOGI(TAG, "Starting continuous VAD processing...");
    ESP_LOGI(TAG, "Press Ctrl+] to exit monitor");
    
    while (1) {
        // Read audio data from I2S
        esp_err_t ret = i2s_channel_read(rx_handle, audio_buffer, 
                                         VAD_HOP_SIZE * sizeof(int16_t), 
                                         &bytes_read, portMAX_DELAY);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(ret));
            continue;
        }
        
        if (bytes_read != VAD_HOP_SIZE * sizeof(int16_t)) {
            ESP_LOGW(TAG, "Incomplete frame: %d bytes", bytes_read);
            continue;
        }
        
        // Process with VAD
        ret = ten_vad_process(vad_handle, audio_buffer, VAD_HOP_SIZE,
                             &probability, &voice_detected);
        
        if (ret == 0) {
            frame_count++;
            if (voice_detected) {
                voice_frames++;
                ESP_LOGI(TAG, "🎤 VOICE [%u] p=%.3f", frame_count, probability);
            } else {
                // Uncomment to see all frames (can be verbose)
                // ESP_LOGD(TAG, "   silence [%u] p=%.3f", frame_count, probability);
            }
            
            // Report statistics every 5 seconds
            uint64_t current_time = esp_timer_get_time();
            if (current_time - last_report_time >= 5000000) { // 5 seconds
                float voice_percentage = (float)voice_frames / frame_count * 100;
                ESP_LOGI(TAG, "=== Statistics (last 5s) ===");
                ESP_LOGI(TAG, "Total frames: %u", frame_count);
                ESP_LOGI(TAG, "Voice frames: %u (%.1f%%)", voice_frames, voice_percentage);
                ESP_LOGI(TAG, "============================");
                
                // Reset counters
                frame_count = 0;
                voice_frames = 0;
                last_report_time = current_time;
            }
        } else {
            ESP_LOGE(TAG, "VAD processing failed: %d", ret);
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
    
    // Print memory info
    ESP_LOGI(TAG, "Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    // Create VAD processing task
    xTaskCreatePinnedToCore(
        vad_task,           // Task function
        "vad_task",         // Task name
        8192,               // Stack size
        NULL,               // Parameters
        5,                  // Priority
        NULL,               // Task handle
        1                   // Core ID (1 for APP CPU)
    );
    
    ESP_LOGI(TAG, "System running. VAD is active.");
}
