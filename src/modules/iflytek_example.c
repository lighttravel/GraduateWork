/**
 * @file iflytek_example.c
 * @brief 科大讯飞ASR集成示例
 *
 * 这个文件展示了如何在main_new.c中集成科大讯飞语音听写
 */

#include "esp_log.h"
#include "modules/iflytek_asr.h"
#include "middleware/audio_manager.h"

static const char *TAG = "IFLYTEK_EXAMPLE";

// ==================== 配置信息 ====================
// 请替换为你的科大讯飞认证信息
#define IFLYTEK_APPID        "9ed12221"
#define IFLYTEK_API_KEY      "b1ffeca6c122160445ebd4a4d69003b4"
#define IFLYTEK_API_SECRET   "NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4"

// ==================== 事件回调 ====================

/**
 * @brief 科大讯飞ASR事件回调
 */
static void iflytek_asr_event_handler(iflytek_asr_event_t event,
                                       const iflytek_asr_result_t *result,
                                       void *user_data)
{
    switch (event) {
        case IFLYTEK_ASR_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✓ 已连接到科大讯飞服务器");
            break;

        case IFLYTEK_ASR_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "✗ 与科大讯飞服务器断开连接");
            break;

        case IFLYTEK_ASR_EVENT_LISTENING_START:
            ESP_LOGI(TAG, "🎤 开始听写，请说话...");
            break;

        case IFLYTEK_ASR_EVENT_LISTENING_STOP:
            ESP_LOGI(TAG, "🛑 听写已停止");
            break;

        case IFLYTEK_ASR_EVENT_RESULT_PARTIAL:
            if (result && result->text[0] != '\0') {
                ESP_LOGI(TAG, "💬 临时结果: %s", result->text);
            }
            break;

        case IFLYTEK_ASR_EVENT_RESULT_FINAL:
            if (result && result->text[0] != '\0') {
                ESP_LOGI(TAG, "✅ 最终结果: %s (置信度: %d%%)",
                         result->text, result->confidence);

                // TODO: 在这里处理识别结果
                // 例如：发送给对话模块、控制设备等
                // process_recognition_result(result->text);
            }
            break;

        case IFLYTEK_ASR_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ ASR发生错误");
            // 可以在这里实现错误恢复逻辑
            break;

        default:
            break;
    }
}

// ==================== 集成到main_new.c的说明 ====================

/*
 * 在main_new.c中集成科大讯飞ASR的步骤：

 * 1. 包含头文件
 *    #include "modules/iflytek_asr.h"

 * 2. 在app_main()中添加初始化代码：

    // 初始化科大讯飞ASR
    iflytek_asr_config_t iflytek_config = {
        .appid = IFLYTEK_APPID,
        .api_key = IFLYTEK_API_KEY,
        .api_secret = IFLYTEK_API_SECRET,
        .language = "zh_cn",        // 中文
        .domain = "iat",          // 通用听写
        .enable_punctuation = true,  // 开启标点
        .enable_nlp = true,          // 开启NLP优化
        .sample_rate = 16000,        // 16kHz
    };

    ret = iflytek_asr_init(&iflytek_config, iflytek_asr_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "科大讯飞ASR初始化失败");
    }

    // 连接到科大讯飞服务器
    ret = iflytek_asr_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "连接科大讯飞服务器失败");
    }

 * 3. 修改音频数据回调，将数据发送给科大讯飞ASR：

    static void audio_data_callback(uint8_t *data, size_t len, void *user_data) {
        // ... 原有代码 ...

        // 如果科大讯飞ASR正在听写，发送音频数据
        if (iflytek_asr_is_listening()) {
            iflytek_asr_send_audio(data, len);
        }
    }

 * 4. 在唤醒词检测回调中开始听写：

    static void wakenet_event_callback(wakenet_event_t event, ...) {
        switch (event) {
            case WAKENET_EVENT_DETECTED:
                // ... 原有代码 ...

                // 开始科大讯飞听写
                iflytek_asr_start_listening();
                break;
        }
    }

 * 5. 处理识别结果（在事件回调中）：

    static void iflytek_asr_event_handler(...) {
        switch (event) {
            case IFLYTEK_ASR_EVENT_RESULT_FINAL:
                if (result) {
                    // 将识别文本发送给对话模块
                    chat_module_send_text(result->text);
                }
                break;
        }
    }

 */

// ==================== 测试函数 ====================

/**
 * @brief 简单的测试函数，可以在命令行中调用
 */
void iflytek_test(void)
{
    ESP_LOGI(TAG, "========== 科大讯飞ASR测试 ==========");

    // 检查初始化状态
    if (g_ctx == NULL) {
        ESP_LOGE(TAG, "科大讯飞ASR未初始化");
        return;
    }

    // 检查连接状态
    if (!iflytek_asr_is_connected()) {
        ESP_LOGW(TAG, "未连接到服务器，尝试连接...");
        esp_err_t ret = iflytek_asr_connect();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "连接失败: %s", esp_err_to_name(ret));
            return;
        }
        // 等待连接完成
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // 开始听写
    ESP_LOGI(TAG, "开始听写测试，请说话...");
    esp_err_t ret = iflytek_asr_start_listening();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "开始听写失败: %s", esp_err_to_name(ret));
        return;
    }

    // 听写5秒
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 停止听写
    ESP_LOGI(TAG, "停止听写");
    iflytek_asr_stop_listening();

    ESP_LOGI(TAG, "========== 测试结束 ==========");
}

#endif // IFLYTEK_EXAMPLE_C
