#include "es8311_codec.h"

#include <stdlib.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "es8311.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "ES8311_V2";

#define DMA_DESC_NUM 8
#define DMA_FRAME_NUM 480
#define MONO_TO_STEREO_BUF_SIZE 512
#define RX_CLOCK_CHUNK_FRAMES 256
#define WARMUP_FRAMES 240

struct es8311_codec_v2_handle {
    i2s_chan_handle_t tx_handle;
    i2s_chan_handle_t rx_handle;
    es8311_handle_t codec;

    i2c_port_t i2c_port;
    gpio_num_t i2c_sda_pin;
    gpio_num_t i2c_scl_pin;
    uint32_t i2c_freq_hz;
    uint8_t i2c_addr;
    bool i2c_ready;

    int input_sample_rate;
    int output_sample_rate;
    gpio_num_t pa_pin;
    bool pa_inverted;
    int volume;
    float input_gain;
    bool input_enabled;
    bool output_enabled;
    bool tx_active;
    bool rx_active;
    bool muted;
    bool use_mclk;

    SemaphoreHandle_t mutex;
};

static void set_pa_enable(es8311_codec_v2_handle_t handle, bool enable)
{
    if (handle->pa_pin == GPIO_NUM_NC) {
        return;
    }

    int level = enable ? 1 : 0;
    if (handle->pa_inverted) {
        level = !level;
    }

    if (enable) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    gpio_set_level(handle->pa_pin, level);

    if (!enable) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static esp_err_t configure_pa_gpio(es8311_codec_v2_handle_t handle)
{
    if (handle->pa_pin == GPIO_NUM_NC) {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << handle->pa_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "configure pa gpio failed");
    set_pa_enable(handle, false);
    return ESP_OK;
}

static esp_err_t set_channel_state(i2s_chan_handle_t channel, bool *active, bool enable)
{
    if (channel == NULL || active == NULL || *active == enable) {
        return ESP_OK;
    }

    esp_err_t ret = enable ? i2s_channel_enable(channel) : i2s_channel_disable(channel);
    if (ret == ESP_OK) {
        *active = enable;
    }
    return ret;
}

static esp_err_t probe_addr(i2c_port_t port, uint8_t addr)
{
    uint8_t reg = 0x00;
    uint8_t value = 0;
    return i2c_master_write_read_device(
        port,
        addr,
        &reg,
        sizeof(reg),
        &value,
        sizeof(value),
        pdMS_TO_TICKS(1000));
}

static esp_err_t init_i2c(es8311_codec_v2_handle_t handle)
{
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = handle->i2c_sda_pin,
        .scl_io_num = handle->i2c_scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = handle->i2c_freq_hz,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(handle->i2c_port, &i2c_cfg), TAG, "i2c_param_config failed");

    esp_err_t ret = i2c_driver_install(handle->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    handle->i2c_ready = true;
    return ESP_OK;
}

static esp_err_t detect_codec_addr(es8311_codec_v2_handle_t handle)
{
    const uint8_t requested = handle->i2c_addr;
    const uint8_t candidates[] = {requested, ES8311_ADDRESS_0, ES8311_ADDRESS_1};

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        uint8_t addr = candidates[i];
        if (addr == 0) {
            continue;
        }
        if (i > 0 && addr == requested) {
            continue;
        }
        if (probe_addr(handle->i2c_port, addr) == ESP_OK) {
            handle->i2c_addr = addr;
            ESP_LOGI(TAG, "codec ack at 0x%02X", addr);
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}

static esp_err_t create_i2s_channels(es8311_codec_v2_handle_t handle,
                                     const es8311_codec_v2_config_t *config)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &handle->tx_handle, &handle->rx_handle),
                        TAG,
                        "i2s_new_channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)config->output_sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = config->use_mclk ? config->mclk_pin : GPIO_NUM_NC,
            .bclk = config->bclk_pin,
            .ws = config->ws_pin,
            .dout = config->dout_pin,
            .din = config->din_pin,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    std_cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_16BIT;
    std_cfg.slot_cfg.bit_shift = true;
    std_cfg.slot_cfg.left_align = false;
    std_cfg.clk_cfg.mclk_multiple = config->use_mclk ? I2S_MCLK_MULTIPLE_256 : I2S_MCLK_MULTIPLE_128;

    esp_err_t ret = i2s_channel_init_std_mode(handle->tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = i2s_channel_init_std_mode(handle->rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static es8311_mic_gain_t map_input_gain(float gain_db)
{
    static const float gain_steps[] = {0.0f, 6.0f, 12.0f, 18.0f, 24.0f, 30.0f, 36.0f, 42.0f};

    if (gain_db <= gain_steps[0]) {
        return ES8311_MIC_GAIN_0DB;
    }

    for (size_t i = 1; i < sizeof(gain_steps) / sizeof(gain_steps[0]); ++i) {
        if (gain_db <= gain_steps[i]) {
            return (es8311_mic_gain_t)i;
        }
    }

    return ES8311_MIC_GAIN_42DB;
}

static esp_err_t init_codec(es8311_codec_v2_handle_t handle)
{
    es8311_clock_config_t clk_cfg = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = handle->use_mclk,
        .mclk_frequency = handle->output_sample_rate * (handle->use_mclk ? 256 : 0),
        .sample_frequency = handle->output_sample_rate,
    };

    handle->codec = es8311_create(handle->i2c_port, handle->i2c_addr);
    ESP_RETURN_ON_FALSE(handle->codec != NULL, ESP_ERR_NO_MEM, TAG, "es8311_create failed");

    ESP_RETURN_ON_ERROR(es8311_init(handle->codec, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16),
                        TAG,
                        "es8311_init failed");
    ESP_RETURN_ON_ERROR(es8311_microphone_config(handle->codec, false), TAG, "mic config failed");
    ESP_RETURN_ON_ERROR(es8311_microphone_gain_set(handle->codec, map_input_gain(handle->input_gain)),
                        TAG,
                        "mic gain failed");
    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(handle->codec, handle->volume, NULL), TAG, "volume set failed");
    ESP_RETURN_ON_ERROR(es8311_voice_mute(handle->codec, true), TAG, "initial mute failed");

    handle->muted = true;
    return ESP_OK;
}

static void warmup_output(es8311_codec_v2_handle_t handle)
{
    if (handle->tx_handle == NULL) {
        return;
    }

    int16_t silence[WARMUP_FRAMES * 2] = {0};
    size_t written = 0;

    for (int i = 0; i < 4; ++i) {
        (void)i2s_channel_write(handle->tx_handle, silence, sizeof(silence), &written, pdMS_TO_TICKS(100));
    }
}

static void cleanup_codec(es8311_codec_v2_handle_t handle)
{
    if (handle->codec != NULL) {
        (void)es8311_voice_mute(handle->codec, true);
        es8311_delete(handle->codec);
        handle->codec = NULL;
    }
}

esp_err_t es8311_codec_v2_create(const es8311_codec_v2_config_t *config,
                                 es8311_codec_v2_handle_t *p_handle)
{
    if (config == NULL || p_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    es8311_codec_v2_handle_t handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    handle->i2c_port = config->i2c_port;
    handle->i2c_sda_pin = config->i2c_sda_pin;
    handle->i2c_scl_pin = config->i2c_scl_pin;
    handle->i2c_freq_hz = config->i2c_freq_hz;
    handle->i2c_addr = config->i2c_addr;
    handle->input_sample_rate = config->input_sample_rate;
    handle->output_sample_rate = config->output_sample_rate;
    handle->pa_pin = config->pa_pin;
    handle->pa_inverted = config->pa_inverted;
    handle->volume = config->default_volume;
    handle->input_gain = config->input_gain;
    handle->use_mclk = config->use_mclk;

    handle->mutex = xSemaphoreCreateMutex();
    if (handle->mutex == NULL) {
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = configure_pa_gpio(handle);
    if (ret == ESP_OK) {
        ret = init_i2c(handle);
    }
    if (ret == ESP_OK) {
        ret = detect_codec_addr(handle);
    }
    if (ret == ESP_OK) {
        ret = create_i2s_channels(handle, config);
    }
    if (ret == ESP_OK) {
        ret = init_codec(handle);
    }

    if (ret != ESP_OK) {
        es8311_codec_v2_destroy(handle);
        return ret;
    }

    if (handle->input_sample_rate != handle->output_sample_rate) {
        ESP_LOGW(TAG,
                 "codec shares one clock domain, input=%d output=%d, using output sample rate",
                 handle->input_sample_rate,
                 handle->output_sample_rate);
    }

    *p_handle = handle;
    return ESP_OK;
}

esp_err_t es8311_codec_v2_destroy(es8311_codec_v2_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)es8311_codec_v2_enable_input(handle, false);
    (void)es8311_codec_v2_enable_output(handle, false);

    cleanup_codec(handle);

    if (handle->tx_handle != NULL) {
        (void)i2s_channel_disable(handle->tx_handle);
        (void)i2s_del_channel(handle->tx_handle);
        handle->tx_handle = NULL;
    }
    if (handle->rx_handle != NULL) {
        (void)i2s_channel_disable(handle->rx_handle);
        (void)i2s_del_channel(handle->rx_handle);
        handle->rx_handle = NULL;
    }

    if (handle->i2c_ready) {
        (void)i2c_driver_delete(handle->i2c_port);
        handle->i2c_ready = false;
    }

    if (handle->mutex != NULL) {
        vSemaphoreDelete(handle->mutex);
    }

    free(handle);
    return ESP_OK;
}

esp_err_t es8311_codec_v2_enable_input(es8311_codec_v2_handle_t handle, bool enable)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->input_enabled = enable;

    esp_err_t ret = set_channel_state(handle->rx_handle, &handle->rx_active, enable);
    if (ret == ESP_OK && enable) {
        ret = set_channel_state(handle->tx_handle, &handle->tx_active, true);
        if (ret == ESP_OK) {
            warmup_output(handle);
        }
    } else if (ret == ESP_OK && !handle->output_enabled) {
        ret = set_channel_state(handle->tx_handle, &handle->tx_active, false);
    }

    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t es8311_codec_v2_enable_output(es8311_codec_v2_handle_t handle, bool enable)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->output_enabled = enable;

    esp_err_t ret = set_channel_state(handle->tx_handle, &handle->tx_active, enable || handle->input_enabled);
    if (ret == ESP_OK && enable) {
        warmup_output(handle);
        ret = es8311_voice_mute(handle->codec, false);
        if (ret == ESP_OK) {
            handle->muted = false;
            set_pa_enable(handle, true);
        }
    } else if (ret == ESP_OK) {
        set_pa_enable(handle, false);
        ret = es8311_voice_mute(handle->codec, true);
        if (ret == ESP_OK) {
            handle->muted = true;
        }
    }

    xSemaphoreGive(handle->mutex);
    return ret;
}

int es8311_codec_v2_read(es8311_codec_v2_handle_t handle, int16_t *buffer, int samples)
{
    if (handle == NULL || buffer == NULL || samples <= 0) {
        return -1;
    }

    if (!handle->input_enabled || !handle->rx_active) {
        return 0;
    }

    static const int16_t silence[RX_CLOCK_CHUNK_FRAMES * 2] = {0};
    int16_t stereo_buf[RX_CLOCK_CHUNK_FRAMES * 2];
    int total_samples = 0;

    while (total_samples < samples) {
        int chunk = samples - total_samples;
        if (chunk > RX_CLOCK_CHUNK_FRAMES) {
            chunk = RX_CLOCK_CHUNK_FRAMES;
        }

        if (!handle->output_enabled) {
            size_t clock_written = 0;
            size_t clock_bytes = (size_t)chunk * 2U * sizeof(int16_t);
            esp_err_t ret = i2s_channel_write(handle->tx_handle,
                                              silence,
                                              clock_bytes,
                                              &clock_written,
                                              pdMS_TO_TICKS(1000));
            if (ret != ESP_OK || clock_written != clock_bytes) {
                ESP_LOGW(TAG, "i2s clock write failed: %s written=%u expected=%u",
                         esp_err_to_name(ret),
                         (unsigned)clock_written,
                         (unsigned)clock_bytes);
                return -1;
            }
        }

        size_t bytes_read = 0;
        size_t bytes_to_read = (size_t)chunk * 2U * sizeof(int16_t);
        esp_err_t ret = i2s_channel_read(handle->rx_handle,
                                         stereo_buf,
                                         bytes_to_read,
                                         &bytes_read,
                                         pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "i2s read failed: %s", esp_err_to_name(ret));
            return -1;
        }

        int frames_read = (int)(bytes_read / (2U * sizeof(int16_t)));
        if (frames_read <= 0) {
            break;
        }

        for (int i = 0; i < frames_read; ++i) {
            buffer[total_samples + i] = stereo_buf[i * 2];
        }

        total_samples += frames_read;
    }

    return total_samples;
}

int es8311_codec_v2_write(es8311_codec_v2_handle_t handle, const int16_t *data, int samples)
{
    if (handle == NULL || data == NULL || samples <= 0) {
        return -1;
    }

    if (!handle->output_enabled || !handle->tx_active) {
        return 0;
    }

    int16_t stereo_buf[MONO_TO_STEREO_BUF_SIZE * 2];
    int total_written = 0;
    int offset = 0;

    while (offset < samples) {
        int chunk = samples - offset;
        if (chunk > MONO_TO_STEREO_BUF_SIZE) {
            chunk = MONO_TO_STEREO_BUF_SIZE;
        }

        for (int i = 0; i < chunk; ++i) {
            int16_t sample = data[offset + i];
            stereo_buf[i * 2] = sample;
            stereo_buf[i * 2 + 1] = sample;
        }

        size_t bytes_written = 0;
        size_t chunk_bytes = (size_t)chunk * 2U * sizeof(int16_t);
        esp_err_t ret = i2s_channel_write(handle->tx_handle,
                                          stereo_buf,
                                          chunk_bytes,
                                          &bytes_written,
                                          pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2s write failed: %s", esp_err_to_name(ret));
            return -1;
        }

        total_written += (int)(bytes_written / sizeof(int16_t) / 2U);
        offset += chunk;
    }

    return total_written;
}

esp_err_t es8311_codec_v2_set_volume(es8311_codec_v2_handle_t handle, int volume)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (volume < 0) {
        volume = 0;
    }
    if (volume > 100) {
        volume = 100;
    }

    handle->volume = volume;
    return es8311_voice_volume_set(handle->codec, volume, NULL);
}

esp_err_t es8311_codec_v2_get_volume(es8311_codec_v2_handle_t handle, int *volume)
{
    if (handle == NULL || volume == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *volume = handle->volume;
    return ESP_OK;
}

esp_err_t es8311_codec_v2_set_input_gain(es8311_codec_v2_handle_t handle, float gain_db)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->input_gain = gain_db;
    return es8311_microphone_gain_set(handle->codec, map_input_gain(gain_db));
}

esp_err_t es8311_codec_v2_set_mute(es8311_codec_v2_handle_t handle, bool mute)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = es8311_voice_mute(handle->codec, mute);
    if (ret != ESP_OK) {
        return ret;
    }

    handle->muted = mute;
    if (mute) {
        set_pa_enable(handle, false);
    } else if (handle->output_enabled) {
        set_pa_enable(handle, true);
    }

    return ESP_OK;
}

int es8311_codec_v2_get_sample_rate(es8311_codec_v2_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }

    return handle->output_sample_rate;
}

void es8311_codec_v2_dump_registers(es8311_codec_v2_handle_t handle)
{
    if (handle == NULL || handle->codec == NULL) {
        return;
    }

    es8311_register_dump(handle->codec);
}
