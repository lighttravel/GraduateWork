/**
 * @file wakenet_module.h
 * @brief WakeNet wrapper for ESP-SR wake word detection.
 */

#ifndef WAKENET_MODULE_H
#define WAKENET_MODULE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WAKENET_MODULE_STATE_IDLE = 0,
    WAKENET_MODULE_STATE_LISTENING,
    WAKENET_MODULE_STATE_DETECTED,
    WAKENET_MODULE_STATE_ERROR
} wakenet_module_state_t;

typedef enum {
    WAKENET_EVENT_LISTENING_START = 0,
    WAKENET_EVENT_LISTENING_STOP,
    WAKENET_EVENT_DETECTED,
    WAKENET_EVENT_ERROR
} wakenet_event_t;

typedef struct {
    float score;
    uint32_t timestamp;
} wakenet_result_t;

typedef void (*wakenet_event_callback_t)(wakenet_event_t event, const wakenet_result_t *result, void *user_data);

typedef struct {
    char wake_word[32];
    int sample_rate;
    bool vad_enable;
    float vad_threshold;
    bool aec_enable;
    bool aec_filter;
} wakenet_config_t;

esp_err_t wakenet_init(const wakenet_config_t *config, wakenet_event_callback_t event_cb, void *user_data);
esp_err_t wakenet_deinit(void);

esp_err_t wakenet_start(void);
esp_err_t wakenet_stop(void);
esp_err_t wakenet_process_audio(const int16_t *data, size_t len);

wakenet_module_state_t wakenet_get_state(void);
bool wakenet_is_listening(void);
float wakenet_get_last_score(void);

esp_err_t wakenet_set_vad_threshold(float threshold);
esp_err_t wakenet_set_aec(bool enable);

#ifdef __cplusplus
}
#endif

#endif
