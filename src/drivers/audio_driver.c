#include "audio_driver.h"

#include <stdlib.h>
#include <string.h>

#include "es8311_codec_v2.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_DRV";

struct audio_driver_handle {
    es8311_codec_v2_handle_t codec;
    audio_driver_config_t config;
    bool is_recording;
    bool is_playing;
};

esp_err_t audio_driver_create(const audio_driver_config_t *config,
                              audio_driver_handle_t *p_handle)
{
    if (config == NULL || p_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_driver_handle_t handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&handle->config, config, sizeof(*config));

    es8311_codec_v2_config_t codec_config = {
        .i2c_port = I2C_NUM_0,
        .i2c_sda_pin = config->i2c_sda_pin,
        .i2c_scl_pin = config->i2c_scl_pin,
        .i2c_freq_hz = config->i2c_freq_hz,
        .i2c_addr = config->es8311_addr,
        .mclk_pin = config->i2s_mclk_pin,
        .bclk_pin = config->i2s_bclk_pin,
        .ws_pin = config->i2s_ws_pin,
        .dout_pin = config->i2s_dout_pin,
        .din_pin = config->i2s_din_pin,
        .pa_pin = config->pa_pin,
        .pa_inverted = config->pa_inverted,
        .input_sample_rate = config->input_sample_rate,
        .output_sample_rate = config->output_sample_rate,
        .default_volume = config->default_volume,
        .input_gain = config->input_gain,
        .use_mclk = config->use_mclk,
        .use_filter = config->use_filter,
    };

    esp_err_t ret = es8311_codec_v2_create(&codec_config, &handle->codec);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "codec create failed: %s", esp_err_to_name(ret));
        free(handle);
        return ret;
    }

    *p_handle = handle;
    return ESP_OK;
}

esp_err_t audio_driver_destroy(audio_driver_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)audio_driver_stop_recording(handle);
    (void)audio_driver_stop_playback(handle);

    if (handle->codec != NULL) {
        (void)es8311_codec_v2_destroy(handle->codec);
    }

    free(handle);
    return ESP_OK;
}

esp_err_t audio_driver_start_recording(audio_driver_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->is_recording) {
        return ESP_OK;
    }

    esp_err_t ret = es8311_codec_v2_enable_input(handle->codec, true);
    if (ret == ESP_OK) {
        handle->is_recording = true;
    }
    return ret;
}

esp_err_t audio_driver_stop_recording(audio_driver_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!handle->is_recording) {
        return ESP_OK;
    }

    esp_err_t ret = es8311_codec_v2_enable_input(handle->codec, false);
    handle->is_recording = false;
    return ret;
}

esp_err_t audio_driver_start_playback(audio_driver_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->is_playing) {
        return ESP_OK;
    }

    esp_err_t ret = es8311_codec_v2_enable_output(handle->codec, true);
    if (ret == ESP_OK) {
        handle->is_playing = true;
    }
    return ret;
}

esp_err_t audio_driver_stop_playback(audio_driver_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!handle->is_playing) {
        return ESP_OK;
    }

    esp_err_t ret = es8311_codec_v2_enable_output(handle->codec, false);
    handle->is_playing = false;
    return ret;
}

int audio_driver_read(audio_driver_handle_t handle, int16_t *buffer, int samples)
{
    if (handle == NULL || buffer == NULL || samples <= 0) {
        return -1;
    }

    return es8311_codec_v2_read(handle->codec, buffer, samples);
}

int audio_driver_write(audio_driver_handle_t handle, const int16_t *data, int samples)
{
    if (handle == NULL || data == NULL || samples <= 0) {
        return -1;
    }

    return es8311_codec_v2_write(handle->codec, data, samples);
}

esp_err_t audio_driver_set_volume(audio_driver_handle_t handle, int volume)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return es8311_codec_v2_set_volume(handle->codec, volume);
}

esp_err_t audio_driver_get_volume(audio_driver_handle_t handle, int *volume)
{
    if (handle == NULL || volume == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return es8311_codec_v2_get_volume(handle->codec, volume);
}

esp_err_t audio_driver_set_mute(audio_driver_handle_t handle, bool mute)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return es8311_codec_v2_set_mute(handle->codec, mute);
}

bool audio_driver_is_device_ready(audio_driver_handle_t handle)
{
    return handle != NULL && handle->codec != NULL;
}

int audio_driver_get_sample_rate(audio_driver_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }

    return es8311_codec_v2_get_sample_rate(handle->codec);
}

void audio_driver_dump_codec_registers(audio_driver_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    es8311_codec_v2_dump_registers(handle->codec);
}
