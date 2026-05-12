#ifndef ES8311_CODEC_V2_H
#define ES8311_CODEC_V2_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t i2c_port;
    gpio_num_t i2c_sda_pin;
    gpio_num_t i2c_scl_pin;
    uint32_t i2c_freq_hz;
    uint8_t i2c_addr;

    gpio_num_t mclk_pin;
    gpio_num_t bclk_pin;
    gpio_num_t ws_pin;
    gpio_num_t dout_pin;
    gpio_num_t din_pin;

    gpio_num_t pa_pin;
    bool pa_inverted;

    int input_sample_rate;
    int output_sample_rate;
    int default_volume;
    float input_gain;

    bool use_mclk;
    bool use_filter;
} es8311_codec_v2_config_t;

typedef struct es8311_codec_v2_handle *es8311_codec_v2_handle_t;

esp_err_t es8311_codec_v2_create(const es8311_codec_v2_config_t *config,
                                 es8311_codec_v2_handle_t *p_handle);
esp_err_t es8311_codec_v2_destroy(es8311_codec_v2_handle_t handle);

esp_err_t es8311_codec_v2_enable_input(es8311_codec_v2_handle_t handle, bool enable);
esp_err_t es8311_codec_v2_enable_output(es8311_codec_v2_handle_t handle, bool enable);

int es8311_codec_v2_read(es8311_codec_v2_handle_t handle, int16_t *buffer, int samples);
int es8311_codec_v2_write(es8311_codec_v2_handle_t handle, const int16_t *data, int samples);

esp_err_t es8311_codec_v2_set_volume(es8311_codec_v2_handle_t handle, int volume);
esp_err_t es8311_codec_v2_get_volume(es8311_codec_v2_handle_t handle, int *volume);
esp_err_t es8311_codec_v2_set_input_gain(es8311_codec_v2_handle_t handle, float gain_db);
esp_err_t es8311_codec_v2_set_mute(es8311_codec_v2_handle_t handle, bool mute);

int es8311_codec_v2_get_sample_rate(es8311_codec_v2_handle_t handle);
void es8311_codec_v2_dump_registers(es8311_codec_v2_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
