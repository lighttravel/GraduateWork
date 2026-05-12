#include "audio_service.h"

#include <stdlib.h>

#include "app_config.h"
#include "audio_board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO_MGR";

typedef struct {
    audio_config_t config;
    audio_state_t state;
    uint8_t volume;
    bool vad_enabled;
    uint16_t vad_threshold;
    bool stop_record_requested;
    audio_driver_handle_t driver;
    TaskHandle_t record_task;
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
    uint64_t sum = 0;

    for (size_t i = 0; i < samples; ++i) {
        int32_t sample = data[i];
        sum += (uint64_t)(sample * sample);
    }

    return samples == 0 ? 0 : (uint32_t)(sum / samples);
}

static bool detect_voice_activity(const int16_t *data, size_t samples)
{
    if (g_audio_mgr == NULL || !g_audio_mgr->vad_enabled) {
        return true;
    }

    return calculate_audio_energy(data, samples) > g_audio_mgr->vad_threshold;
}

static bool should_record_task_continue(void)
{
    bool should_continue = false;

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);
    should_continue = g_audio_mgr->state == AUDIO_STATE_RECORDING && !g_audio_mgr->stop_record_requested;
    xSemaphoreGive(g_audio_mgr->mutex);

    return should_continue;
}

static void record_task(void *pv_parameters)
{
    (void)pv_parameters;

    int16_t buffer[640];
    const int samples = (int)(sizeof(buffer) / sizeof(buffer[0]));

    ESP_LOGI(TAG, "record task started");

    while (should_record_task_continue()) {
        const int samples_read = audio_driver_read(g_audio_mgr->driver, buffer, samples);

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

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);
    g_audio_mgr->record_task = NULL;
    if (g_audio_mgr->state == AUDIO_STATE_RECORDING) {
        g_audio_mgr->state = AUDIO_STATE_IDLE;
    }
    xSemaphoreGive(g_audio_mgr->mutex);

    ESP_LOGI(TAG, "record task stopped");
    vTaskDelete(NULL);
}

esp_err_t audio_manager_init(const audio_config_t *config)
{
    if (g_audio_mgr != NULL) {
        return ESP_OK;
    }
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    g_audio_mgr = calloc(1, sizeof(*g_audio_mgr));
    if (g_audio_mgr == NULL) {
        return ESP_ERR_NO_MEM;
    }

    g_audio_mgr->config = *config;
    g_audio_mgr->state = AUDIO_STATE_IDLE;
    g_audio_mgr->volume = config->volume;
    g_audio_mgr->vad_enabled = config->vad_enabled;
    g_audio_mgr->vad_threshold = config->vad_threshold;
    g_audio_mgr->mutex = xSemaphoreCreateMutex();

    if (g_audio_mgr->mutex == NULL) {
        free(g_audio_mgr);
        g_audio_mgr = NULL;
        return ESP_ERR_NO_MEM;
    }

    const audio_driver_config_t driver_config = {
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

    const esp_err_t ret = audio_driver_create(&driver_config, &g_audio_mgr->driver);
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

    const esp_err_t ret = audio_manager_stop_record();
    if (ret != ESP_OK) {
        return ret;
    }

    (void)audio_driver_destroy(g_audio_mgr->driver);
    vSemaphoreDelete(g_audio_mgr->mutex);
    free(g_audio_mgr);
    g_audio_mgr = NULL;
    return ESP_OK;
}

esp_err_t audio_manager_start_record(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);
    if (g_audio_mgr->state == AUDIO_STATE_RECORDING) {
        xSemaphoreGive(g_audio_mgr->mutex);
        return ESP_OK;
    }
    if (g_audio_mgr->state != AUDIO_STATE_IDLE) {
        xSemaphoreGive(g_audio_mgr->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = audio_driver_start_recording(g_audio_mgr->driver);
    if (ret == ESP_OK) {
        g_audio_mgr->stop_record_requested = false;
        g_audio_mgr->state = AUDIO_STATE_RECORDING;
    }
    xSemaphoreGive(g_audio_mgr->mutex);

    if (ret != ESP_OK) {
        return ret;
    }

    if (xTaskCreate(record_task, "record", AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY,
                    &g_audio_mgr->record_task) != pdPASS) {
        (void)audio_manager_stop_record();
        return ESP_FAIL;
    }

    notify_event(AUDIO_EVENT_RECORD_START);
    return ESP_OK;
}

esp_err_t audio_manager_stop_record(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_OK;
    }

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);
    if (g_audio_mgr->state != AUDIO_STATE_RECORDING) {
        xSemaphoreGive(g_audio_mgr->mutex);
        return ESP_OK;
    }
    g_audio_mgr->stop_record_requested = true;
    xSemaphoreGive(g_audio_mgr->mutex);

    for (int i = 0; g_audio_mgr->record_task != NULL && i < 300; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (g_audio_mgr->record_task != NULL) {
        ESP_LOGE(TAG, "record task stop timeout");
        return ESP_ERR_TIMEOUT;
    }

    (void)audio_driver_stop_recording(g_audio_mgr->driver);

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);
    g_audio_mgr->state = AUDIO_STATE_IDLE;
    xSemaphoreGive(g_audio_mgr->mutex);

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
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_stop_play(void)
{
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
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_stop_tts_playback(void)
{
    return ESP_OK;
}

esp_err_t audio_manager_start_playback(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_stop_playback(void)
{
    return ESP_OK;
}

esp_err_t audio_manager_play_tts_audio(const uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_set_volume(uint8_t volume)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    g_audio_mgr->volume = volume;
    notify_event(AUDIO_EVENT_VOLUME_CHANGED);
    return audio_driver_set_volume(g_audio_mgr->driver, volume);
}

uint8_t audio_manager_get_volume(void)
{
    return g_audio_mgr == NULL ? 0 : g_audio_mgr->volume;
}

audio_state_t audio_manager_get_state(void)
{
    return g_audio_mgr == NULL ? AUDIO_STATE_IDLE : g_audio_mgr->state;
}

bool audio_manager_is_recording(void)
{
    return audio_manager_get_state() == AUDIO_STATE_RECORDING;
}

bool audio_manager_is_playing(void)
{
    return false;
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
    if (g_audio_mgr != NULL) {
        audio_driver_dump_codec_registers(g_audio_mgr->driver);
    }
}
