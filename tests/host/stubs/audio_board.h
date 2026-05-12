#ifndef AUDIO_BOARD_H
#define AUDIO_BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

typedef struct audio_driver_handle *audio_driver_handle_t;

typedef struct {
    gpio_num_t i2c_sda_pin;
    gpio_num_t i2c_scl_pin;
    uint32_t i2c_freq_hz;
    gpio_num_t i2s_mclk_pin;
    gpio_num_t i2s_bclk_pin;
    gpio_num_t i2s_ws_pin;
    gpio_num_t i2s_dout_pin;
    gpio_num_t i2s_din_pin;
    gpio_num_t pa_pin;
    bool pa_inverted;
    int input_sample_rate;
    int output_sample_rate;
    int default_volume;
    float input_gain;
    bool use_mclk;
    bool use_filter;
    uint8_t es8311_addr;
} audio_driver_config_t;

static inline esp_err_t audio_driver_create(const audio_driver_config_t *config, audio_driver_handle_t *handle) { (void)config; *handle = (audio_driver_handle_t)1; return ESP_OK; }
static inline esp_err_t audio_driver_destroy(audio_driver_handle_t handle) { (void)handle; return ESP_OK; }
static inline esp_err_t audio_driver_start_recording(audio_driver_handle_t handle) { (void)handle; return ESP_OK; }
static inline esp_err_t audio_driver_stop_recording(audio_driver_handle_t handle) { (void)handle; return ESP_OK; }
static inline esp_err_t audio_driver_start_playback(audio_driver_handle_t handle) { (void)handle; return ESP_OK; }
static inline esp_err_t audio_driver_stop_playback(audio_driver_handle_t handle) { (void)handle; return ESP_OK; }
static inline int audio_driver_read(audio_driver_handle_t handle, int16_t *buffer, int samples) { (void)handle; (void)buffer; (void)samples; return 0; }
static inline int audio_driver_write(audio_driver_handle_t handle, const int16_t *data, int samples) { (void)handle; (void)data; return samples; }
static inline esp_err_t audio_driver_set_volume(audio_driver_handle_t handle, int volume) { (void)handle; (void)volume; return ESP_OK; }
static inline void audio_driver_dump_codec_registers(audio_driver_handle_t handle) { (void)handle; }

#endif
