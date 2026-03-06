#ifndef ES8311_DIRECT_TONE_H
#define ES8311_DIRECT_TONE_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t es8311_direct_play_test_tone(uint32_t duration_ms);
esp_err_t es8311_direct_init_playback(void);
esp_err_t es8311_direct_write_tts_pcm(const uint8_t *pcm_data, size_t len);
void es8311_direct_deinit_playback(void);

#ifdef __cplusplus
}
#endif

#endif
