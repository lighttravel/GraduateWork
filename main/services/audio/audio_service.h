#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_STATE_IDLE = 0,
    AUDIO_STATE_RECORDING,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PROCESSING,
} audio_state_t;

typedef enum {
    AUDIO_EVENT_RECORD_START = 0,
    AUDIO_EVENT_RECORD_STOP,
    AUDIO_EVENT_PLAY_START,
    AUDIO_EVENT_PLAY_STOP,
    AUDIO_EVENT_VOLUME_CHANGED,
    AUDIO_EVENT_ERROR,
} audio_event_t;

typedef void (*audio_data_callback_t)(uint8_t *data, size_t len, void *user_data);
typedef void (*audio_event_callback_t)(audio_event_t event, void *user_data);

typedef struct {
    uint32_t sample_rate;
    uint8_t volume;
    bool vad_enabled;
    uint16_t vad_threshold;
    audio_data_callback_t data_cb;
    audio_event_callback_t event_cb;
    void *user_data;
} audio_config_t;

esp_err_t audio_manager_init(const audio_config_t *config);
esp_err_t audio_manager_deinit(void);

esp_err_t audio_manager_start_record(void);
esp_err_t audio_manager_stop_record(void);
esp_err_t audio_manager_pause_record(void);
esp_err_t audio_manager_resume_record(void);

esp_err_t audio_manager_start_play(const uint8_t *data, size_t len);
esp_err_t audio_manager_stop_play(void);
esp_err_t audio_manager_pause_play(void);
esp_err_t audio_manager_resume_play(void);

esp_err_t audio_manager_start_tts_playback(void);
esp_err_t audio_manager_stop_tts_playback(void);
esp_err_t audio_manager_start_playback(void);
esp_err_t audio_manager_stop_playback(void);
esp_err_t audio_manager_play_tts_audio(const uint8_t *data, size_t len);

esp_err_t audio_manager_set_volume(uint8_t volume);
uint8_t audio_manager_get_volume(void);

audio_state_t audio_manager_get_state(void);
bool audio_manager_is_recording(void);
bool audio_manager_is_playing(void);

esp_err_t audio_manager_set_vad(bool enabled);
esp_err_t audio_manager_set_vad_threshold(uint16_t threshold);

void audio_manager_dump_codec_registers(void);

#ifdef __cplusplus
}
#endif

#endif
