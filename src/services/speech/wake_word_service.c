/**
 * @file wakenet_module.c
 * @brief Real WakeNet integration based on ESP-SR.
 */

#include "wake_word_service.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"

static const char *TAG = "WAKENET";

#define DEFAULT_WAKE_MODEL_KEYWORD "nihaoxiaozhi"
#define DEFAULT_SAMPLE_RATE        16000
#define MODEL_PARTITION_LABEL      "model"
#define DETECTION_MODE             DET_MODE_95

typedef struct {
    wakenet_module_state_t state;
    wakenet_event_callback_t event_cb;
    void *user_data;
    wakenet_config_t config;

    srmodel_list_t *models;
    const esp_wn_iface_t *iface;
    model_iface_data_t *model_data;
    char model_name[MODEL_NAME_MAX_LENGTH];

    int16_t *feed_buffer;
    int feed_chunk_size;
    int feed_buffer_fill;

    float det_threshold;
    float last_score;
    uint32_t total_samples;
    uint32_t last_timestamp;
} wakenet_module_t;

static wakenet_module_t *g_wakenet;

static void fill_default_config(wakenet_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->sample_rate = DEFAULT_SAMPLE_RATE;
    config->vad_enable = true;
    config->vad_threshold = 0.5f;
    strlcpy(config->wake_word, DEFAULT_WAKE_MODEL_KEYWORD, sizeof(config->wake_word));
}

static float map_threshold(float threshold)
{
    if (threshold < 0.0f) {
        threshold = 0.0f;
    }
    if (threshold > 1.0f) {
        threshold = 1.0f;
    }

    return 0.48f + (threshold * 0.24f);
}

static const char *pick_model_keyword(const wakenet_config_t *config)
{
    if (config == NULL || config->wake_word[0] == '\0') {
        return DEFAULT_WAKE_MODEL_KEYWORD;
    }

    if (strstr(config->wake_word, "xiaozhi") != NULL) {
        return DEFAULT_WAKE_MODEL_KEYWORD;
    }

    return config->wake_word;
}

static esp_err_t apply_detection_threshold(wakenet_module_t *ctx)
{
    int word_num = 1;

    if (ctx == NULL || ctx->iface == NULL || ctx->model_data == NULL || ctx->iface->set_det_threshold == NULL) {
        return ESP_OK;
    }

    if (ctx->iface->get_word_num != NULL) {
        word_num = ctx->iface->get_word_num(ctx->model_data);
    }

    ctx->det_threshold = map_threshold(ctx->config.vad_threshold);

    for (int word_index = 1; word_index <= word_num; ++word_index) {
        (void)ctx->iface->set_det_threshold(ctx->model_data, ctx->det_threshold, word_index);
    }

    ESP_LOGI(TAG, "WakeNet threshold set to %.3f", ctx->det_threshold);
    return ESP_OK;
}

static esp_err_t recreate_model_instance(wakenet_module_t *ctx)
{
    int new_chunk_size;

    if (ctx == NULL || ctx->iface == NULL || ctx->model_name[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    if (ctx->model_data != NULL) {
        ctx->iface->destroy(ctx->model_data);
        ctx->model_data = NULL;
    }

    ctx->model_data = ctx->iface->create(ctx->model_name, DETECTION_MODE);
    if (ctx->model_data == NULL) {
        ESP_LOGE(TAG, "WakeNet create(%s) failed", ctx->model_name);
        return ESP_FAIL;
    }

    new_chunk_size = ctx->iface->get_samp_chunksize(ctx->model_data);
    if (new_chunk_size <= 0) {
        ESP_LOGE(TAG, "Invalid WakeNet chunk size: %d", new_chunk_size);
        return ESP_FAIL;
    }

    if (new_chunk_size != ctx->feed_chunk_size || ctx->feed_buffer == NULL) {
        int16_t *new_buffer = calloc((size_t)new_chunk_size, sizeof(int16_t));
        if (new_buffer == NULL) {
            ESP_LOGE(TAG, "WakeNet buffer allocation failed");
            return ESP_ERR_NO_MEM;
        }

        free(ctx->feed_buffer);
        ctx->feed_buffer = new_buffer;
        ctx->feed_chunk_size = new_chunk_size;
    }

    if (ctx->config.sample_rate <= 0) {
        ctx->config.sample_rate = ctx->iface->get_samp_rate(ctx->model_data);
    }

    ctx->feed_buffer_fill = 0;
    return apply_detection_threshold(ctx);
}

static esp_err_t load_wakenet_model(wakenet_module_t *ctx)
{
    char *model_name = NULL;
    char *wake_words = NULL;
    const char *keyword = pick_model_keyword(&ctx->config);

    ctx->models = esp_srmodel_init(MODEL_PARTITION_LABEL);
    if (ctx->models == NULL) {
        ESP_LOGE(TAG, "esp_srmodel_init(%s) failed", MODEL_PARTITION_LABEL);
        return ESP_FAIL;
    }

    model_name = esp_srmodel_filter(ctx->models, ESP_WN_PREFIX, keyword);
    if (model_name == NULL && strcmp(keyword, DEFAULT_WAKE_MODEL_KEYWORD) != 0) {
        model_name = esp_srmodel_filter(ctx->models, ESP_WN_PREFIX, DEFAULT_WAKE_MODEL_KEYWORD);
    }
    if (model_name == NULL) {
        model_name = esp_srmodel_filter(ctx->models, ESP_WN_PREFIX, NULL);
    }
    if (model_name == NULL) {
        ESP_LOGE(TAG, "No WakeNet model found in partition '%s'", MODEL_PARTITION_LABEL);
        return ESP_ERR_NOT_FOUND;
    }

    ctx->iface = esp_wn_handle_from_name(model_name);
    if (ctx->iface == NULL) {
        ESP_LOGE(TAG, "esp_wn_handle_from_name(%s) failed", model_name);
        return ESP_FAIL;
    }

    strlcpy(ctx->model_name, model_name, sizeof(ctx->model_name));
    ESP_RETURN_ON_ERROR(recreate_model_instance(ctx), TAG, "Failed to create WakeNet instance");

    wake_words = esp_srmodel_get_wake_words(ctx->models, model_name);
    ESP_LOGI(TAG, "WakeNet model: %s", model_name);
    ESP_LOGI(TAG, "Wake word(s): %s", wake_words ? wake_words : "unknown");
    ESP_LOGI(TAG, "WakeNet sample_rate=%d chunk=%d", ctx->config.sample_rate, ctx->feed_chunk_size);
    return ESP_OK;
}

static void unload_wakenet_model(wakenet_module_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->iface != NULL && ctx->model_data != NULL) {
        ctx->iface->destroy(ctx->model_data);
        ctx->model_data = NULL;
    }

    free(ctx->feed_buffer);
    ctx->feed_buffer = NULL;
    ctx->feed_chunk_size = 0;
    ctx->feed_buffer_fill = 0;
    ctx->iface = NULL;

    if (ctx->models != NULL) {
        esp_srmodel_deinit(ctx->models);
        ctx->models = NULL;
    }
}

esp_err_t wakenet_init(const wakenet_config_t *config, wakenet_event_callback_t event_cb, void *user_data)
{
    esp_err_t err;

    if (g_wakenet != NULL) {
        return ESP_OK;
    }

    g_wakenet = calloc(1, sizeof(*g_wakenet));
    if (g_wakenet == NULL) {
        return ESP_ERR_NO_MEM;
    }

    fill_default_config(&g_wakenet->config);
    if (config != NULL) {
        g_wakenet->config = *config;
        if (g_wakenet->config.sample_rate <= 0) {
            g_wakenet->config.sample_rate = DEFAULT_SAMPLE_RATE;
        }
        if (g_wakenet->config.wake_word[0] == '\0') {
            strlcpy(g_wakenet->config.wake_word, DEFAULT_WAKE_MODEL_KEYWORD, sizeof(g_wakenet->config.wake_word));
        }
    }

    g_wakenet->event_cb = event_cb;
    g_wakenet->user_data = user_data;
    g_wakenet->state = WAKENET_MODULE_STATE_IDLE;

    err = load_wakenet_model(g_wakenet);
    if (err != ESP_OK) {
        unload_wakenet_model(g_wakenet);
        free(g_wakenet);
        g_wakenet = NULL;
        return err;
    }

    return ESP_OK;
}

esp_err_t wakenet_deinit(void)
{
    if (g_wakenet == NULL) {
        return ESP_OK;
    }

    (void)wakenet_stop();
    unload_wakenet_model(g_wakenet);
    free(g_wakenet);
    g_wakenet = NULL;
    return ESP_OK;
}

esp_err_t wakenet_start(void)
{
    if (g_wakenet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(recreate_model_instance(g_wakenet), TAG, "Failed to reset WakeNet instance");

    g_wakenet->feed_buffer_fill = 0;
    g_wakenet->total_samples = 0;
    g_wakenet->last_timestamp = 0;
    g_wakenet->last_score = 0.0f;
    g_wakenet->state = WAKENET_MODULE_STATE_LISTENING;

    if (g_wakenet->event_cb != NULL) {
        g_wakenet->event_cb(WAKENET_EVENT_LISTENING_START, NULL, g_wakenet->user_data);
    }

    ESP_LOGI(TAG, "WakeNet listening started");
    return ESP_OK;
}

esp_err_t wakenet_stop(void)
{
    if (g_wakenet == NULL || g_wakenet->state == WAKENET_MODULE_STATE_IDLE) {
        return ESP_OK;
    }

    g_wakenet->feed_buffer_fill = 0;
    g_wakenet->state = WAKENET_MODULE_STATE_IDLE;

    if (g_wakenet->event_cb != NULL) {
        g_wakenet->event_cb(WAKENET_EVENT_LISTENING_STOP, NULL, g_wakenet->user_data);
    }

    ESP_LOGI(TAG, "WakeNet listening stopped");
    return ESP_OK;
}

esp_err_t wakenet_process_audio(const int16_t *data, size_t len)
{
    if (g_wakenet == NULL || g_wakenet->state != WAKENET_MODULE_STATE_LISTENING) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    while (len > 0 && g_wakenet->state == WAKENET_MODULE_STATE_LISTENING) {
        size_t copy_count = (size_t)(g_wakenet->feed_chunk_size - g_wakenet->feed_buffer_fill);
        if (copy_count > len) {
            copy_count = len;
        }

        memcpy(&g_wakenet->feed_buffer[g_wakenet->feed_buffer_fill], data, copy_count * sizeof(int16_t));
        g_wakenet->feed_buffer_fill += (int)copy_count;
        g_wakenet->total_samples += (uint32_t)copy_count;
        data += copy_count;
        len -= copy_count;

        if (g_wakenet->feed_buffer_fill == g_wakenet->feed_chunk_size) {
            int detect_result = (int)g_wakenet->iface->detect(g_wakenet->model_data, g_wakenet->feed_buffer);
            g_wakenet->feed_buffer_fill = 0;

            if (detect_result > 0) {
                const char *word_name = NULL;

                if (g_wakenet->iface->get_word_name != NULL) {
                    word_name = g_wakenet->iface->get_word_name(g_wakenet->model_data, detect_result);
                }

                g_wakenet->state = WAKENET_MODULE_STATE_DETECTED;
                g_wakenet->last_score = 1.0f;
                g_wakenet->last_timestamp = g_wakenet->total_samples / (uint32_t)g_wakenet->config.sample_rate;

                ESP_LOGI(TAG, "Wake word detected: index=%d name=%s", detect_result, word_name ? word_name : "unknown");

                if (g_wakenet->event_cb != NULL) {
                    wakenet_result_t result = {
                        .score = g_wakenet->last_score,
                        .timestamp = g_wakenet->last_timestamp,
                    };
                    g_wakenet->event_cb(WAKENET_EVENT_DETECTED, &result, g_wakenet->user_data);
                }
            }
        }
    }

    return ESP_OK;
}

wakenet_module_state_t wakenet_get_state(void)
{
    return g_wakenet ? g_wakenet->state : WAKENET_MODULE_STATE_IDLE;
}

bool wakenet_is_listening(void)
{
    return wakenet_get_state() == WAKENET_MODULE_STATE_LISTENING;
}

float wakenet_get_last_score(void)
{
    return g_wakenet ? g_wakenet->last_score : 0.0f;
}

esp_err_t wakenet_set_vad_threshold(float threshold)
{
    if (g_wakenet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (threshold < 0.0f || threshold > 1.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    g_wakenet->config.vad_threshold = threshold;
    return apply_detection_threshold(g_wakenet);
}

esp_err_t wakenet_set_aec(bool enable)
{
    if (g_wakenet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    g_wakenet->config.aec_enable = enable;
    return ESP_OK;
}
