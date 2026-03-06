/**
 * @file wakenet_module.c
 * @brief 唤醒词检测模块实现 - 存根实现
 *
 * 注意: 这是一个简化的存根实现，基于能量检测的唤醒词检测。
 *
 * 完整ESP-SR集成需要：
 * 1. 添加ESP-SR组件: idf.py add-dependency "espressif/esp-sr^^1.0.0"
 * 2. 配置SPI Flash分区存储模型文件
 * 3. 参考官方示例: https://github.com/espressif/esp-sr
 *
 * 临时方案: 使用VAD (Voice Activity Detection) + 能量检测
 */

#include "wakenet_module.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WAKENET_MODULE";

// 唤醒词模块上下文
typedef struct {
    wakenet_state_t state;
    wakenet_event_callback_t event_cb;
    void *user_data;

    // 配置
    wakenet_config_t config;

    // 能量检测相关
    float energy_threshold;      // 能量阈值
    uint32_t sample_count;      // 样本计数
    uint32_t speech_frames;     // 语音帧计数
    uint32_t silence_frames;    // 静音帧计数

    // 最后一次检测
    float last_score;
    uint32_t last_timestamp;

} wakenet_module_t;

static wakenet_module_t *g_wakenet = NULL;

// ==================== 辅助函数 ====================

/**
 * @brief 计算音频能量 (RMS)
 */
static float calculate_energy_rms(const int16_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return 0.0f;
    }

    int64_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        int64_t sample = data[i];
        sum += sample * sample;
    }

    return (float)sqrt(sum / len);
}

// ==================== 初始化和配置 ====================

esp_err_t wakenet_init(const wakenet_config_t *config, wakenet_event_callback_t event_cb, void *user_data)
{
    if (g_wakenet != NULL) {
        ESP_LOGW(TAG, "唤醒词模块已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化唤醒词检测模块 (存根实现)");

    // 分配内存
    g_wakenet = (wakenet_module_t *)calloc(1, sizeof(wakenet_module_t));
    if (g_wakenet == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存配置
    if (config) {
        memcpy(&g_wakenet->config, config, sizeof(wakenet_config_t));
    } else {
        // 默认配置
        strncpy(g_wakenet->config.wake_word, "hinet_xiaozhi", 31);
        g_wakenet->config.sample_rate = 16000;
        g_wakenet->config.vad_enable = true;
        g_wakenet->config.vad_threshold = 0.5f;
    }

    g_wakenet->event_cb = event_cb;
    g_wakenet->user_data = user_data;
    g_wakenet->state = WAKENET_STATE_IDLE;

    // 计算能量阈值 (基于VAD阈值)
    // VAD_THRESHOLD = 500 (来自config.h)
    g_wakenet->energy_threshold = VAD_THRESHOLD * 2.0f;  // 简单的2倍系数

    ESP_LOGI(TAG, "唤醒词: %s", g_wakenet->config.wake_word);
    ESP_LOGI(TAG, "能量阈值: %.2f", g_wakenet->energy_threshold);
    ESP_LOGW(TAG, "注意: 这是存根实现，基于VAD能量检测，不是真正的唤醒词识别");

    return ESP_OK;
}

esp_err_t wakenet_deinit(void)
{
    if (g_wakenet == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化唤醒词检测模块");

    wakenet_stop();

    free(g_wakenet);
    g_wakenet = NULL;

    return ESP_OK;
}

// ==================== 控制接口 ====================

esp_err_t wakenet_start(void)
{
    if (g_wakenet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_wakenet->state == WAKENET_STATE_LISTENING) {
        ESP_LOGW(TAG, "已在监听状态");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "开始监听唤醒词");
    g_wakenet->state = WAKENET_STATE_LISTENING;
    g_wakenet->sample_count = 0;
    g_wakenet->speech_frames = 0;
    g_wakenet->silence_frames = 0;

    // 触发回调
    if (g_wakenet->event_cb) {
        g_wakenet->event_cb(WAKENET_EVENT_LISTENING_START, NULL, g_wakenet->user_data);
    }

    return ESP_OK;
}

esp_err_t wakenet_stop(void)
{
    if (g_wakenet == NULL || g_wakenet->state == WAKENET_STATE_IDLE) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止监听唤醒词");
    g_wakenet->state = WAKENET_STATE_IDLE;

    // 触发回调
    if (g_wakenet->event_cb) {
        g_wakenet->event_cb(WAKENET_EVENT_LISTENING_STOP, NULL, g_wakenet->user_data);
    }

    return ESP_OK;
}

esp_err_t wakenet_process_audio(const int16_t *data, size_t len)
{
    if (g_wakenet == NULL || g_wakenet->state != WAKENET_STATE_LISTENING) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 计算音频能量
    float energy = calculate_energy_rms(data, len);
    g_wakenet->sample_count += len;

    // VAD检测: 判断是否为语音
    bool is_speech = (energy > g_wakenet->energy_threshold);

    if (is_speech) {
        g_wakenet->speech_frames++;
        g_wakenet->silence_frames = 0;

        // 简化的唤醒词检测逻辑:
        // 检测到持续语音超过一定帧数，认为可能是唤醒词
        // TODO: 实际应该使用ESP-SR的WakeNet模型

        if (g_wakenet->speech_frames > 50) {  // 约0.5秒的语音 (16kHz, 512帧/次)
            // 模拟检测到唤醒词
            g_wakenet->state = WAKENET_STATE_DETECTED;
            g_wakenet->last_score = 0.8f + (energy / 10000.0f);  // 模拟置信度
            if (g_wakenet->last_score > 1.0f) g_wakenet->last_score = 1.0f;
            g_wakenet->last_timestamp = g_wakenet->sample_count / g_wakenet->config.sample_rate;

            ESP_LOGI(TAG, "检测到唤醒词! 置信度: %.2f", g_wakenet->last_score);

            // 触发回调
            if (g_wakenet->event_cb) {
                wakenet_result_t result = {
                    .score = g_wakenet->last_score,
                    .timestamp = g_wakenet->last_timestamp
                };
                g_wakenet->event_cb(WAKENET_EVENT_DETECTED, &result, g_wakenet->user_data);
            }

            // 重置计数器以准备下次检测
            g_wakenet->speech_frames = 0;
        }
    } else {
        g_wakenet->silence_frames++;

        // 如果静音持续，重置语音帧计数
        if (g_wakenet->silence_frames > 20) {  // 约200ms静音
            g_wakenet->speech_frames = 0;
        }
    }

    return ESP_OK;
}

// ==================== 状态查询 ====================

wakenet_state_t wakenet_get_state(void)
{
    return g_wakenet ? g_wakenet->state : WAKENET_STATE_IDLE;
}

bool wakenet_is_listening(void)
{
    wakenet_state_t state = wakenet_get_state();
    return (state == WAKENET_STATE_LISTENING);
}

float wakenet_get_last_score(void)
{
    return g_wakenet ? g_wakenet->last_score : 0.0f;
}

// ==================== 配置 ====================

esp_err_t wakenet_set_vad_threshold(float threshold)
{
    if (g_wakenet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (threshold < 0.0f || threshold > 1.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    g_wakenet->config.vad_threshold = threshold;
    g_wakenet->energy_threshold = threshold * VAD_THRESHOLD * 2.0f;

    ESP_LOGI(TAG, "VAD阈值更新为: %.2f, 能量阈值: %.2f", threshold, g_wakenet->energy_threshold);
    return ESP_OK;
}

esp_err_t wakenet_set_aec(bool enable)
{
    if (g_wakenet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    g_wakenet->config.aec_enable = enable;
    ESP_LOGI(TAG, "AEC %s", enable ? "启用" : "禁用");
    return ESP_OK;
}

// ==================== TODO说明 ====================

/*
 * 完整ESP-SR集成步骤:
 *
 * 1. 添加ESP-SR组件:
 *    idf.py add-dependency "espressif/esp-sr^1.0.0"
 *    或在platformio.ini的lib_deps中添加:
 *    https://github.com/espressif/esp-sr.git
 *
 * 2. 下载模型文件到SPI Flash:
 *    - WakeNet模型: wakenet_model.bin
 *    - 从ESP-SR SDK获取模型文件
 *    - 使用partitions.csv添加模型分区
 *
 * 3. 参考官方示例代码:
 *    https://github.com/espressif/esp-sr/blob/master/examples/simple_wakenet/main/main.c
 *
 * 4. 使用ESP-SR API:
 *    - esp_sr_init()
 *    - esp_sr_process()
 *    - esp_sr_deinit()
 *
 * 5. 参考文档:
 *    - https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/getting_started/readme.html
 *    - https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/wake_word_engine/README.html
 */
