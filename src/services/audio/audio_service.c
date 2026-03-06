#include "audio_service.h"

#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "audio_board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO_MGR";

typedef struct {
    audio_config_t config;
    audio_state_t state;
    uint8_t volume;
    bool vad_enabled;
    uint16_t vad_threshold;

    audio_driver_handle_t driver;

    TaskHandle_t record_task;
    TaskHandle_t play_task;
    TaskHandle_t tts_task;

    uint8_t *play_buffer;
    size_t play_buffer_size;
    size_t play_buffer_offset;

    StreamBufferHandle_t tts_stream;
    bool tts_stop_requested;

    SemaphoreHandle_t mutex;
} audio_manager_t;

static audio_manager_t *g_audio_mgr;

static void notify_event(audio_event_t event)
{
    if (g_audio_mgr != NULL && g_audio_mgr->config.event_cb != NULL) {
        g_audio_mgr->config.event_cb(event, g_audio_mgr->config.user_data);
    }
}

static uint32_t calculate_audio_energy(const int16_t *data, size_t samples)
{
    int64_t sum = 0;

    for (size_t i = 0; i < samples; ++i) {
        sum += (int64_t)data[i] * data[i];
    }

    return (uint32_t)(sum / samples);
}

static bool detect_voice_activity(const int16_t *data, size_t samples)
{
    if (g_audio_mgr == NULL || !g_audio_mgr->vad_enabled) {
        return true;
    }

    return calculate_audio_energy(data, samples) > g_audio_mgr->vad_threshold;
}

static void record_task(void *pv_parameters)
{
    (void)pv_parameters;

    int16_t buffer[640];
    const int samples = (int)(sizeof(buffer) / sizeof(buffer[0]));

    ESP_LOGI(TAG, "record task started");

    while (g_audio_mgr != NULL && g_audio_mgr->state == AUDIO_STATE_RECORDING) {
        int samples_read = audio_driver_read(g_audio_mgr->driver, buffer, samples);

        if (samples_read < 0) {
            ESP_LOGE(TAG, "audio read failed");
            notify_event(AUDIO_EVENT_ERROR);
            break;
        }

        if (samples_read > 0 && detect_voice_activity(buffer, (size_t)samples_read) &&
            g_audio_mgr->config.data_cb != NULL) {
            g_audio_mgr->config.data_cb(
                (uint8_t *)buffer,
                (size_t)samples_read * sizeof(int16_t),
                g_audio_mgr->config.user_data);
        }
    }

    if (g_audio_mgr != NULL) {
        g_audio_mgr->record_task = NULL;
        if (g_audio_mgr->state == AUDIO_STATE_RECORDING) {
            g_audio_mgr->state = AUDIO_STATE_IDLE;
        }
    }

    ESP_LOGI(TAG, "record task stopped");
    vTaskDelete(NULL);
}

static void play_task(void *pv_parameters)
{
    (void)pv_parameters;

    ESP_LOGI(TAG, "play task started");

    while (g_audio_mgr != NULL && g_audio_mgr->state == AUDIO_STATE_PLAYING) {
        if (g_audio_mgr->play_buffer_offset >= g_audio_mgr->play_buffer_size) {
            break;
        }

        size_t remaining = g_audio_mgr->play_buffer_size - g_audio_mgr->play_buffer_offset;
        int samples_to_write = (int)((remaining < 1024U * sizeof(int16_t))
                                         ? (remaining / sizeof(int16_t))
                                         : 1024U);

        int written = audio_driver_write(
            g_audio_mgr->driver,
            (const int16_t *)(g_audio_mgr->play_buffer + g_audio_mgr->play_buffer_offset),
            samples_to_write);

        if (written <= 0) {
            ESP_LOGE(TAG, "audio write failed");
            notify_event(AUDIO_EVENT_ERROR);
            break;
        }

        g_audio_mgr->play_buffer_offset += (size_t)written * sizeof(int16_t);
    }

    if (g_audio_mgr != NULL) {
        free(g_audio_mgr->play_buffer);
        g_audio_mgr->play_buffer = NULL;
        g_audio_mgr->play_buffer_size = 0;
        g_audio_mgr->play_buffer_offset = 0;
        g_audio_mgr->play_task = NULL;
        g_audio_mgr->state = AUDIO_STATE_IDLE;
        (void)audio_driver_stop_playback(g_audio_mgr->driver);
    }

    notify_event(AUDIO_EVENT_PLAY_STOP);
    ESP_LOGI(TAG, "play task stopped");
    vTaskDelete(NULL);
}

static void tts_play_task(void *pv_parameters)
{
    (void)pv_parameters;

    uint8_t pcm_chunk[AUDIO_TTS_WRITE_CHUNK_BYTES];
    bool playback_started = false;

    ESP_LOGI(TAG, "tts stream task started");

    while (g_audio_mgr != NULL) {
        size_t available = xStreamBufferBytesAvailable(g_audio_mgr->tts_stream);

        if (!playback_started) {
            if (g_audio_mgr->tts_stop_requested && available == 0U) {
                break;
            }
            if (available < AUDIO_TTS_PREROLL_BYTES) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            playback_started = true;
        }

        size_t received = xStreamBufferReceive(
            g_audio_mgr->tts_stream,
            pcm_chunk,
            sizeof(pcm_chunk),
            pdMS_TO_TICKS(AUDIO_TTS_CHUNK_WAIT_MS));

        if (received == 0U) {
            if (g_audio_mgr->tts_stop_requested &&
                xStreamBufferBytesAvailable(g_audio_mgr->tts_stream) == 0U) {
                break;
            }
            continue;
        }

        received &= ~((size_t)1U);
        if (received == 0U) {
            continue;
        }

        const int16_t *samples = (const int16_t *)pcm_chunk;
        int total_samples = (int)(received / sizeof(int16_t));
        int offset = 0;

        while (offset < total_samples) {
            int written = audio_driver_write(g_audio_mgr->driver, samples + offset, total_samples - offset);
            if (written <= 0) {
                ESP_LOGE(TAG, "tts audio write failed");
                notify_event(AUDIO_EVENT_ERROR);
                g_audio_mgr->tts_stop_requested = true;
                break;
            }
            offset += written;
        }
    }

    if (g_audio_mgr != NULL) {
        g_audio_mgr->tts_task = NULL;
    }

    ESP_LOGI(TAG, "tts stream task stopped");
    vTaskDelete(NULL);
}

static void cleanup_tts_stream(void)
{
    if (g_audio_mgr != NULL && g_audio_mgr->tts_stream != NULL) {
        vStreamBufferDelete(g_audio_mgr->tts_stream);
        g_audio_mgr->tts_stream = NULL;
    }
}

esp_err_t audio_manager_init(const audio_config_t *config)
{
    if (g_audio_mgr != NULL) {
        return ESP_OK;
    }

    g_audio_mgr = (audio_manager_t *)calloc(1, sizeof(audio_manager_t));
    if (g_audio_mgr == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (config != NULL) {
        g_audio_mgr->config = *config;
    }

    g_audio_mgr->state = AUDIO_STATE_IDLE;
    g_audio_mgr->volume = config != NULL ? config->volume : AUDIO_VOLUME;
    g_audio_mgr->vad_enabled = config != NULL && config->vad_enabled;
    g_audio_mgr->vad_threshold = config != NULL ? config->vad_threshold : VAD_THRESHOLD;
    g_audio_mgr->mutex = xSemaphoreCreateMutex();

    if (g_audio_mgr->mutex == NULL) {
        free(g_audio_mgr);
        g_audio_mgr = NULL;
        return ESP_ERR_NO_MEM;
    }

    audio_driver_config_t driver_config = {
        .i2c_sda_pin = I2C_SDA_PIN,
        .i2c_scl_pin = I2C_SCL_PIN,
        .i2c_freq_hz = I2C_FREQ_HZ,
        .i2s_mclk_pin = I2S_MCLK_PIN,
        .i2s_bclk_pin = I2S_BCLK_PIN,
        .i2s_ws_pin = I2S_LRCK_PIN,
        .i2s_dout_pin = I2S_DIN_PIN,
        .i2s_din_pin = I2S_DOUT_PIN,
        .pa_pin = AUDIO_PA_ENABLE_PIN,
        .pa_inverted = false,
        .input_sample_rate = I2S_SAMPLE_RATE,
        .output_sample_rate = I2S_SAMPLE_RATE_TTS,
        .default_volume = g_audio_mgr->volume,
        .input_gain = AUDIO_INPUT_GAIN_DB,
        .use_mclk = true,
        .use_filter = false,
        .es8311_addr = ES8311_I2C_ADDR,
    };

    esp_err_t ret = audio_driver_create(&driver_config, &g_audio_mgr->driver);
    if (ret != ESP_OK) {
        vSemaphoreDelete(g_audio_mgr->mutex);
        free(g_audio_mgr);
        g_audio_mgr = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "audio manager initialized");
    return ESP_OK;
}

esp_err_t audio_manager_deinit(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_OK;
    }

    (void)audio_manager_stop_record();
    (void)audio_manager_stop_play();
    (void)audio_manager_stop_tts_playback();
    cleanup_tts_stream();

    if (g_audio_mgr->driver != NULL) {
        (void)audio_driver_destroy(g_audio_mgr->driver);
    }
    if (g_audio_mgr->mutex != NULL) {
        vSemaphoreDelete(g_audio_mgr->mutex);
    }

    free(g_audio_mgr->play_buffer);
    free(g_audio_mgr);
    g_audio_mgr = NULL;
    return ESP_OK;
}

esp_err_t audio_manager_start_record(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (g_audio_mgr->state == AUDIO_STATE_RECORDING) {
        return ESP_OK;
    }

    esp_err_t ret = audio_driver_start_recording(g_audio_mgr->driver);
    if (ret != ESP_OK) {
        return ret;
    }

    g_audio_mgr->state = AUDIO_STATE_RECORDING;
    if (xTaskCreate(record_task, "record", AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY,
                    &g_audio_mgr->record_task) != pdPASS) {
        (void)audio_driver_stop_recording(g_audio_mgr->driver);
        g_audio_mgr->state = AUDIO_STATE_IDLE;
        return ESP_FAIL;
    }

    notify_event(AUDIO_EVENT_RECORD_START);
    return ESP_OK;
}

esp_err_t audio_manager_stop_record(void)
{
    if (g_audio_mgr == NULL || g_audio_mgr->state != AUDIO_STATE_RECORDING) {
        return ESP_OK;
    }

    g_audio_mgr->state = AUDIO_STATE_IDLE;

    for (int i = 0; g_audio_mgr->record_task != NULL && i < 50; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    (void)audio_driver_stop_recording(g_audio_mgr->driver);
    notify_event(AUDIO_EVENT_RECORD_STOP);
    return ESP_OK;
}

esp_err_t audio_manager_pause_record(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_resume_record(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_start_play(const uint8_t *data, size_t len)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_audio_mgr->state == AUDIO_STATE_PLAYING) {
        return ESP_OK;
    }

    g_audio_mgr->play_buffer = (uint8_t *)malloc(len);
    if (g_audio_mgr->play_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(g_audio_mgr->play_buffer, data, len);
    g_audio_mgr->play_buffer_size = len;
    g_audio_mgr->play_buffer_offset = 0;

    esp_err_t ret = audio_driver_start_playback(g_audio_mgr->driver);
    if (ret != ESP_OK) {
        free(g_audio_mgr->play_buffer);
        g_audio_mgr->play_buffer = NULL;
        return ret;
    }

    g_audio_mgr->state = AUDIO_STATE_PLAYING;
    if (xTaskCreate(play_task, "play", AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY,
                    &g_audio_mgr->play_task) != pdPASS) {
        free(g_audio_mgr->play_buffer);
        g_audio_mgr->play_buffer = NULL;
        g_audio_mgr->play_buffer_size = 0;
        g_audio_mgr->play_buffer_offset = 0;
        g_audio_mgr->state = AUDIO_STATE_IDLE;
        (void)audio_driver_stop_playback(g_audio_mgr->driver);
        return ESP_FAIL;
    }

    notify_event(AUDIO_EVENT_PLAY_START);
    return ESP_OK;
}

esp_err_t audio_manager_stop_play(void)
{
    if (g_audio_mgr == NULL || g_audio_mgr->state != AUDIO_STATE_PLAYING) {
        return ESP_OK;
    }

    g_audio_mgr->state = AUDIO_STATE_IDLE;
    (void)audio_driver_stop_playback(g_audio_mgr->driver);

    if (g_audio_mgr->play_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_audio_mgr->play_task = NULL;
    }

    return ESP_OK;
}

esp_err_t audio_manager_pause_play(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_resume_play(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_start_tts_playback(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_audio_mgr->tts_stream == NULL) {
        g_audio_mgr->tts_stream = xStreamBufferCreate(AUDIO_TTS_STREAM_BUFFER_BYTES, sizeof(int16_t));
        if (g_audio_mgr->tts_stream == NULL) {
            return ESP_ERR_NO_MEM;
        }
    } else {
        (void)xStreamBufferReset(g_audio_mgr->tts_stream);
    }

    g_audio_mgr->tts_stop_requested = false;

    esp_err_t ret = audio_driver_start_playback(g_audio_mgr->driver);
    if (ret != ESP_OK) {
        return ret;
    }

    g_audio_mgr->state = AUDIO_STATE_PLAYING;

    if (g_audio_mgr->tts_task == NULL &&
        xTaskCreate(tts_play_task, "tts_play", AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY,
                    &g_audio_mgr->tts_task) != pdPASS) {
        (void)audio_driver_stop_playback(g_audio_mgr->driver);
        g_audio_mgr->state = AUDIO_STATE_IDLE;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t audio_manager_play_tts_audio(const uint8_t *data, size_t len)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_audio_mgr->tts_stream == NULL || g_audio_mgr->tts_task == NULL) {
        esp_err_t ret = audio_manager_start_tts_playback();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    size_t total_sent = 0;
    while (total_sent < len) {
        size_t sent = xStreamBufferSend(
            g_audio_mgr->tts_stream,
            data + total_sent,
            len - total_sent,
            pdMS_TO_TICKS(AUDIO_TTS_ENQUEUE_TIMEOUT_MS));

        if (sent == 0U) {
            ESP_LOGW(TAG, "tts buffer full, len=%u queued=%u",
                     (unsigned)len, (unsigned)total_sent);
            return ESP_ERR_TIMEOUT;
        }

        total_sent += sent;
    }

    return ESP_OK;
}

esp_err_t audio_manager_stop_tts_playback(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_OK;
    }

    g_audio_mgr->tts_stop_requested = true;

    if (g_audio_mgr->tts_stream != NULL) {
        for (int elapsed = 0; elapsed < AUDIO_TTS_DRAIN_TIMEOUT_MS; elapsed += 20) {
            if (xStreamBufferBytesAvailable(g_audio_mgr->tts_stream) == 0U && g_audio_mgr->tts_task == NULL) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        (void)xStreamBufferReset(g_audio_mgr->tts_stream);
    }

    if (g_audio_mgr->tts_task != NULL) {
        ESP_LOGW(TAG, "forcing tts task stop");
        vTaskDelete(g_audio_mgr->tts_task);
        g_audio_mgr->tts_task = NULL;
    }

    (void)audio_driver_stop_playback(g_audio_mgr->driver);
    g_audio_mgr->state = AUDIO_STATE_IDLE;
    return ESP_OK;
}

esp_err_t audio_manager_start_playback(void)
{
    return audio_manager_start_tts_playback();
}

esp_err_t audio_manager_stop_playback(void)
{
    return audio_manager_stop_tts_playback();
}

esp_err_t audio_manager_set_volume(uint8_t volume)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (volume > 100U) {
        volume = 100U;
    }

    g_audio_mgr->volume = volume;
    if (audio_driver_set_volume(g_audio_mgr->driver, volume) != ESP_OK) {
        return ESP_FAIL;
    }

    notify_event(AUDIO_EVENT_VOLUME_CHANGED);
    return ESP_OK;
}

uint8_t audio_manager_get_volume(void)
{
    return g_audio_mgr != NULL ? g_audio_mgr->volume : 0U;
}

audio_state_t audio_manager_get_state(void)
{
    return g_audio_mgr != NULL ? g_audio_mgr->state : AUDIO_STATE_IDLE;
}

bool audio_manager_is_recording(void)
{
    return g_audio_mgr != NULL && g_audio_mgr->state == AUDIO_STATE_RECORDING;
}

bool audio_manager_is_playing(void)
{
    return g_audio_mgr != NULL && g_audio_mgr->state == AUDIO_STATE_PLAYING;
}

esp_err_t audio_manager_set_vad(bool enabled)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    g_audio_mgr->vad_enabled = enabled;
    return ESP_OK;
}

esp_err_t audio_manager_set_vad_threshold(uint16_t threshold)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    g_audio_mgr->vad_threshold = threshold;
    return ESP_OK;
}

void audio_manager_dump_codec_registers(void)
{
    if (g_audio_mgr != NULL && g_audio_mgr->driver != NULL) {
        audio_driver_dump_codec_registers(g_audio_mgr->driver);
    }
}
