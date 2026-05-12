#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "audio_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "key_service.h"
#include "ml307r_service.h"

static const char *TAG = "PCB_DEMO";

static esp_err_t init_nvs_storage(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static void audio_event_callback(audio_event_t event, void *user_data)
{
    (void)user_data;

    switch (event) {
        case AUDIO_EVENT_RECORD_START:
            ESP_LOGI(TAG, "record start");
            break;
        case AUDIO_EVENT_RECORD_STOP:
            ESP_LOGI(TAG, "record stop");
            break;
        case AUDIO_EVENT_PLAY_START:
            ESP_LOGI(TAG, "play start");
            break;
        case AUDIO_EVENT_PLAY_STOP:
            ESP_LOGI(TAG, "play stop");
            break;
        case AUDIO_EVENT_ERROR:
            ESP_LOGE(TAG, "audio error");
            break;
        default:
            break;
    }
}

static void audio_data_callback(uint8_t *data, size_t len, void *user_data)
{
    (void)data;
    (void)user_data;
    ESP_LOGI(TAG, "recorded %u bytes", (unsigned)len);
}

static esp_err_t init_audio(void)
{
    const audio_config_t audio_config = {
        .sample_rate = I2S_SAMPLE_RATE,
        .volume = AUDIO_VOLUME,
        .vad_enabled = false,
        .vad_threshold = VAD_THRESHOLD,
        .data_cb = audio_data_callback,
        .event_cb = audio_event_callback,
        .user_data = NULL,
    };

    return audio_manager_init(&audio_config);
}

static void handle_keys(key_state_t *previous_state)
{
    const key_state_t state = key_service_read();

    if (state.key1_pressed && !previous_state->key1_pressed) {
        ESP_LOGI(TAG, "KEY1 pressed: play speaker tone, then start 2s microphone capture");
        esp_err_t tone_ret = audio_manager_play_test_tone(500);
        ESP_LOGI(TAG, "speaker tone %s", tone_ret == ESP_OK ? "OK" : esp_err_to_name(tone_ret));
        if (audio_manager_start_record() == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            (void)audio_manager_stop_record();
        }
    }

    if (state.key2_pressed && !previous_state->key2_pressed) {
        ESP_LOGI(TAG, "KEY2 pressed: run ML307R network diagnostic");
        esp_err_t ret = ml307r_service_check_network();
        ESP_LOGI(TAG, "ML307R network diagnostic %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));
    }

    *previous_state = state;
}

void app_main(void)
{
    printf("\n========================================\n");
    printf(" Graduate PCB ESP32-S3 hardware demo\n");
    printf(" KEY1: speaker tone + microphone capture, KEY2: ML307R network diagnostic\n");
    printf("========================================\n");

    ESP_ERROR_CHECK(init_nvs_storage());
    ESP_ERROR_CHECK(key_service_init());
    ESP_ERROR_CHECK(init_audio());
    ESP_ERROR_CHECK(ml307r_service_init());

    ESP_LOGI(TAG, "hardware demo ready");
    ESP_LOGI(TAG, "ML307R network diagnostic will start in 20 seconds");
    vTaskDelay(pdMS_TO_TICKS(20000));
    esp_err_t diagnostic_ret = ml307r_service_check_network();
    ESP_LOGI(TAG, "ML307R boot network diagnostic %s", diagnostic_ret == ESP_OK ? "OK" : esp_err_to_name(diagnostic_ret));

    key_state_t previous_state = key_service_read();
    while (true) {
        handle_keys(&previous_state);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



