//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// ESP32-S3 Port for TEN VAD

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "ten_vad.h"

static const char *TAG = "TEN_VAD_ESP32";

const int hop_size = 256; // 16 ms per frame at 16kHz

// Test audio sample - small subset for demo
// This is PCM 16-bit audio data at 16kHz sample rate
static const int16_t test_audio_samples[] = {
    -29, -44, -36, -32, -10, -11, -10, -4, -38, -33, -21, -27, -34, -31, -20, -41,
    -44, -46, -33, -22, -10, -4, -1, -7, -15, -15, -2, -4, -14, -8, -18, 2,
    3, 10, 3, -7, -6, -4, 5, -11, -14, 28, 29, 16, 1, 2, -9, -19,
    1, -12, 10, 19, 11, -3, -8, -10, -3, -5, 0, 1, -3, 8, -2, 14,
    -3, 10, 5, -7, -5, 22, 27, 29, 42, 31, 35, 24, 35, 11, 19, 1,
    7, 25, 14, 23, 25, 19, 32, 33, 26, 36, 22, 9, 21, 36, 50, 38,
    33, 62, 45, 47, 31, 55, 35, 23, 24, 16, 25, 16, 47, 6, 16, 11,
    13, -5, 13, 32, 33, 23, 12, 31, 43, 32, 4, 34, 20, 38, 33, 32,
    39, 42, 49, 48, 45, 34, 8, 17, 23, 31, 70, 52, 40, 3, 18, 20,
    26, 31, 24, 16, 13, 28, -2, 15, 40, 30, 22, 27, 26, 21, 22, 33,
    41, 48, 54, 62, 61, 39, 33, 32, 61, 67, 75, 59, 29, 29, 29, 64,
    // Add more frames as needed for testing
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t get_timestamp_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

void print_memory_info(void)
{
    ESP_LOGI(TAG, "=== Memory Information ===");
    ESP_LOGI(TAG, "Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Largest free block (heap): %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "Largest free block (PSRAM): %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "========================");
}

void vad_process_test(void)
{
    ESP_LOGI(TAG, "Starting TEN VAD Test on ESP32-S3");
    ESP_LOGI(TAG, "TEN VAD version: %s", ten_vad_get_version());
    
    print_memory_info();
    
    // Calculate number of frames
    uint32_t sample_count = sizeof(test_audio_samples) / sizeof(test_audio_samples[0]);
    uint32_t frame_count = sample_count / hop_size;
    
    ESP_LOGI(TAG, "Sample count: %u", (unsigned int)sample_count);
    ESP_LOGI(TAG, "Frame count: %u", (unsigned int)frame_count);
    ESP_LOGI(TAG, "Hop size: %d samples (16ms @ 16kHz)", hop_size);
    
    // Allocate output buffers using PSRAM if available
    float *out_probs = (float *)heap_caps_malloc(frame_count * sizeof(float), 
                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int32_t *out_flags = (int32_t *)heap_caps_malloc(frame_count * sizeof(int32_t), 
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (out_probs == NULL || out_flags == NULL) {
        ESP_LOGE(TAG, "Failed to allocate output buffers");
        if (out_probs) heap_caps_free(out_probs);
        if (out_flags) heap_caps_free(out_flags);
        return;
    }
    
    // Create VAD instance
    void *ten_vad_handle = NULL;
    float voice_threshold = 0.5f;
    int ret = ten_vad_create(&ten_vad_handle, hop_size, voice_threshold);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create VAD instance, error: %d", ret);
        heap_caps_free(out_probs);
        heap_caps_free(out_flags);
        return;
    }
    
    ESP_LOGI(TAG, "VAD instance created successfully");
    print_memory_info();
    
    // Process audio frames
    uint64_t start_time = get_timestamp_ms();
    
    for (uint32_t i = 0; i < frame_count; i++) {
        const int16_t *audio_frame = test_audio_samples + (i * hop_size);
        
        ret = ten_vad_process(ten_vad_handle, audio_frame, hop_size,
                             &out_probs[i], &out_flags[i]);
        
        if (ret == 0) {
            ESP_LOGI(TAG, "Frame[%u] probability: %.6f, voice detected: %s",
                    (unsigned int)i, out_probs[i], out_flags[i] ? "YES" : "NO");
        } else {
            ESP_LOGE(TAG, "Frame[%u] processing failed with error: %d",
                    (unsigned int)i, ret);
        }
    }
    
    uint64_t end_time = get_timestamp_ms();
    float processing_time = (float)(end_time - start_time);
    
    // Calculate RTF (Real-Time Factor)
    float audio_duration_ms = (float)sample_count / 16.0f; // 16kHz sample rate
    float rtf = processing_time / audio_duration_ms;
    
    ESP_LOGI(TAG, "=== Performance Results ===");
    ESP_LOGI(TAG, "Processing time: %.2f ms", processing_time);
    ESP_LOGI(TAG, "Audio duration: %.2f ms", audio_duration_ms);
    ESP_LOGI(TAG, "RTF (Real-Time Factor): %.6f", rtf);
    ESP_LOGI(TAG, "=========================");
    
    // Clean up
    ten_vad_destroy(&ten_vad_handle);
    heap_caps_free(out_probs);
    heap_caps_free(out_flags);
    
    ESP_LOGI(TAG, "VAD test completed successfully");
    print_memory_info();
}

void app_main(void)
{
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "TEN VAD ESP32-S3 Demo Application");
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Free heap at startup: %d bytes", esp_get_free_heap_size());
    
    // Check if PSRAM is available
    if (esp_psram_get_size() > 0) {
        ESP_LOGI(TAG, "PSRAM size: %d bytes", esp_psram_get_size());
        ESP_LOGI(TAG, "PSRAM is available and enabled");
    } else {
        ESP_LOGW(TAG, "PSRAM is NOT available!");
    }
    
    ESP_LOGI(TAG, "===================================");
    
    // Run the VAD test
    vad_process_test();
    
    ESP_LOGI(TAG, "Application finished. System will continue running...");
}
