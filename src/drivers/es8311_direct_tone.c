#include "es8311_direct_tone.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "es8311.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ES8311_DIRECT";

#define DIRECT_I2C_PORT       I2C_NUM_0
#define DIRECT_I2C_SCL_PIN    GPIO_NUM_4
#define DIRECT_I2C_SDA_PIN    GPIO_NUM_5
#define DIRECT_I2C_FREQ_HZ    100000
#define DIRECT_CODEC_ADDR0    ES8311_ADDRESS_0
#define DIRECT_CODEC_ADDR1    ES8311_ADDRESS_1

#define DIRECT_I2S_PORT       I2S_NUM_0
#define DIRECT_I2S_MCLK_PIN   GPIO_NUM_6
#define DIRECT_I2S_BCLK_PIN   GPIO_NUM_14
#define DIRECT_I2S_WS_PIN     GPIO_NUM_12
#define DIRECT_I2S_DOUT_PIN   GPIO_NUM_11

#define DIRECT_SAMPLE_RATE_HZ         16000
#define DIRECT_MCLK_MULTIPLE          I2S_MCLK_MULTIPLE_256
#define DIRECT_TONE_FREQ_HZ           1000
#define DIRECT_TONE_AMPLITUDE         8000.0f
#define DIRECT_VOLUME_PERCENT         55
#define DIRECT_PI_F                   3.14159265358979323846f
#define DIRECT_PCM_FRAMES_PER_CHUNK   256U

typedef struct {
    i2s_chan_handle_t tx;
    es8311_handle_t codec;
    uint16_t codec_addr;
    float phase;
    bool i2c_ready;
} direct_ctx_t;

static direct_ctx_t g_stream_ctx = {0};
static bool g_stream_ready = false;

static esp_err_t direct_probe_addr(uint16_t addr)
{
    uint8_t reg = 0x00;
    uint8_t value = 0;
    return i2c_master_write_read_device(
        DIRECT_I2C_PORT,
        addr,
        &reg,
        sizeof(reg),
        &value,
        sizeof(value),
        pdMS_TO_TICKS(1000)
    );
}

static esp_err_t direct_init_i2c(direct_ctx_t *ctx)
{
    const uint16_t codec_addrs[] = {DIRECT_CODEC_ADDR0, DIRECT_CODEC_ADDR1};
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DIRECT_I2C_SDA_PIN,
        .scl_io_num = DIRECT_I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = DIRECT_I2C_FREQ_HZ,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(DIRECT_I2C_PORT, &i2c_cfg), TAG, "i2c_param_config failed");
    ESP_RETURN_ON_ERROR(
        i2c_driver_install(DIRECT_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0),
        TAG,
        "i2c_driver_install failed"
    );

    ctx->i2c_ready = true;
    ctx->codec_addr = 0;

    for (size_t i = 0; i < sizeof(codec_addrs) / sizeof(codec_addrs[0]); ++i) {
        if (direct_probe_addr(codec_addrs[i]) == ESP_OK) {
            ctx->codec_addr = codec_addrs[i];
            ESP_LOGI(TAG, "codec ack at 0x%02X", ctx->codec_addr);
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}

static esp_err_t direct_init_i2s(direct_ctx_t *ctx)
{
    i2s_chan_config_t chan_cfg = {
        .id = DIRECT_I2S_PORT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
    };

    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &ctx->tx, NULL), TAG, "i2s_new_channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = DIRECT_SAMPLE_RATE_HZ,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = DIRECT_MCLK_MULTIPLE,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
        },
        .gpio_cfg = {
            .mclk = DIRECT_I2S_MCLK_PIN,
            .bclk = DIRECT_I2S_BCLK_PIN,
            .ws = DIRECT_I2S_WS_PIN,
            .dout = DIRECT_I2S_DOUT_PIN,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(ctx->tx, &std_cfg), TAG, "i2s init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(ctx->tx), TAG, "i2s enable failed");
    return ESP_OK;
}

static esp_err_t direct_init_codec(direct_ctx_t *ctx)
{
    es8311_clock_config_t clk_cfg = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = DIRECT_SAMPLE_RATE_HZ * 256,
        .sample_frequency = DIRECT_SAMPLE_RATE_HZ,
    };

    ctx->codec = es8311_create(DIRECT_I2C_PORT, ctx->codec_addr);
    ESP_RETURN_ON_FALSE(ctx->codec != NULL, ESP_ERR_NO_MEM, TAG, "es8311_create failed");

    ESP_RETURN_ON_ERROR(
        es8311_init(ctx->codec, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16),
        TAG,
        "es8311_init failed"
    );
    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(ctx->codec, DIRECT_VOLUME_PERCENT, NULL), TAG, "volume set failed");
    ESP_RETURN_ON_ERROR(es8311_voice_mute(ctx->codec, false), TAG, "voice unmute failed");

    ESP_LOGI(TAG, "direct codec path configured by official driver");
    return ESP_OK;
}

static void direct_cleanup(direct_ctx_t *ctx)
{
    if (ctx->codec != NULL) {
        (void)es8311_voice_mute(ctx->codec, true);
        es8311_delete(ctx->codec);
        ctx->codec = NULL;
    }

    if (ctx->tx != NULL) {
        (void)i2s_channel_disable(ctx->tx);
        (void)i2s_del_channel(ctx->tx);
        ctx->tx = NULL;
    }

    if (ctx->i2c_ready) {
        (void)i2c_driver_delete(DIRECT_I2C_PORT);
        ctx->i2c_ready = false;
    }
}

static esp_err_t direct_prepare_output(direct_ctx_t *ctx)
{
    ESP_RETURN_ON_ERROR(direct_init_i2c(ctx), TAG, "direct_init_i2c failed");
    ESP_RETURN_ON_ERROR(direct_init_i2s(ctx), TAG, "direct_init_i2s failed");
    ESP_RETURN_ON_ERROR(direct_init_codec(ctx), TAG, "direct_init_codec failed");
    return ESP_OK;
}

esp_err_t es8311_direct_play_test_tone(uint32_t duration_ms)
{
    direct_ctx_t ctx = {0};
    const uint32_t frames = 480;
    const size_t stereo_samples = frames * 2U;
    const size_t bytes = stereo_samples * sizeof(int16_t);
    const uint32_t total_chunks = duration_ms / 20U;
    const float step = (2.0f * DIRECT_PI_F * (float)DIRECT_TONE_FREQ_HZ) / (float)DIRECT_SAMPLE_RATE_HZ;
    int16_t *buffer = NULL;
    esp_err_t ret = ESP_OK;

    if (duration_ms < 20U) {
        duration_ms = 20U;
    }

    buffer = (int16_t *)calloc(stereo_samples, sizeof(int16_t));
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = direct_prepare_output(&ctx);
    if (ret != ESP_OK) {
        goto done;
    }

    ESP_LOGW(TAG, "DIRECT_PCM_TEST_START duration_ms=%u", (unsigned)duration_ms);

    for (uint32_t chunk = 0; chunk < total_chunks; ++chunk) {
        for (uint32_t i = 0; i < frames; ++i) {
            int16_t sample = (int16_t)(DIRECT_TONE_AMPLITUDE * sinf(ctx.phase));
            buffer[i * 2U] = sample;
            buffer[i * 2U + 1U] = sample;
            ctx.phase += step;
            if (ctx.phase >= 2.0f * DIRECT_PI_F) {
                ctx.phase -= 2.0f * DIRECT_PI_F;
            }
        }

        size_t written = 0;
        ret = i2s_channel_write(ctx.tx, buffer, bytes, &written, pdMS_TO_TICKS(2000));
        if (ret != ESP_OK || written != bytes) {
            ESP_LOGE(TAG, "direct tone write failed ret=%s written=%u expected=%u",
                     esp_err_to_name(ret), (unsigned)written, (unsigned)bytes);
            if (ret == ESP_OK) {
                ret = ESP_FAIL;
            }
            goto done;
        }
    }

    ESP_LOGW(TAG, "DIRECT_PCM_TEST_OK");

done:
    free(buffer);
    direct_cleanup(&ctx);
    return ret;
}

esp_err_t es8311_direct_init_playback(void)
{
    if (g_stream_ready) {
        return ESP_OK;
    }

    memset(&g_stream_ctx, 0, sizeof(g_stream_ctx));
    ESP_RETURN_ON_ERROR(direct_prepare_output(&g_stream_ctx), TAG, "direct_prepare_output failed");

    g_stream_ctx.phase = 0.0f;
    g_stream_ready = true;

    // Prime the DAC path with a short block of silence before real PCM arrives.
    int16_t silence[DIRECT_PCM_FRAMES_PER_CHUNK * 2U] = {0};
    size_t written = 0;
    (void)i2s_channel_write(g_stream_ctx.tx, silence, sizeof(silence), &written, pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "direct streaming playback ready");
    return ESP_OK;
}

esp_err_t es8311_direct_write_tts_pcm(const uint8_t *pcm_data, size_t len)
{
    if (!g_stream_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    if (pcm_data == NULL || len < sizeof(int16_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((len & 0x01U) != 0U) {
        len -= 1U;
    }

    const int16_t *mono = (const int16_t *)pcm_data;
    size_t in_frames = len / sizeof(int16_t);
    size_t idx = 0;
    int16_t stereo_buf[DIRECT_PCM_FRAMES_PER_CHUNK * 2U];
    static uint32_t s_pcm_dbg_count = 0;

    if (in_frames == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    while (idx < in_frames) {
        size_t frames = in_frames - idx;
        if (frames > DIRECT_PCM_FRAMES_PER_CHUNK) {
            frames = DIRECT_PCM_FRAMES_PER_CHUNK;
        }

        int16_t min_sample = 32767;
        int16_t max_sample = -32768;

        for (size_t i = 0; i < frames; ++i) {
            int16_t sample = mono[idx + i];
            stereo_buf[i * 2U] = sample;
            stereo_buf[i * 2U + 1U] = sample;

            if (sample < min_sample) {
                min_sample = sample;
            }
            if (sample > max_sample) {
                max_sample = sample;
            }
        }

        s_pcm_dbg_count++;
        if (s_pcm_dbg_count <= 3U || (s_pcm_dbg_count % 50U) == 0U) {
            ESP_LOGI(TAG, "tts pcm chunk=%lu frames=%u min=%d max=%d",
                     (unsigned long)s_pcm_dbg_count,
                     (unsigned)frames,
                     (int)min_sample,
                     (int)max_sample);
        }

        size_t out_bytes = frames * 2U * sizeof(int16_t);
        size_t written = 0;
        esp_err_t ret = i2s_channel_write(g_stream_ctx.tx, stereo_buf, out_bytes, &written, pdMS_TO_TICKS(2000));
        if (ret != ESP_OK || written != out_bytes) {
            ESP_LOGE(TAG, "direct TTS write failed ret=%s written=%u expected=%u",
                     esp_err_to_name(ret), (unsigned)written, (unsigned)out_bytes);
            if (ret == ESP_OK) {
                return ESP_FAIL;
            }
            return ret;
        }

        idx += frames;
    }

    return ESP_OK;
}

void es8311_direct_deinit_playback(void)
{
    if (!g_stream_ready) {
        return;
    }

    direct_cleanup(&g_stream_ctx);
    memset(&g_stream_ctx, 0, sizeof(g_stream_ctx));
    g_stream_ready = false;
}
