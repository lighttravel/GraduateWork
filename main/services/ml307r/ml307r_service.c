#include "ml307r_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef ML307R_DIAG_TEST
#include "board_config.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static const char *TAG = "ML307R";

typedef struct {
    const char *name;
    const char *command;
    int timeout_ms;
} at_command_t;

typedef struct {
    bool module_ok;
    bool sim_ready;
    bool signal_known;
    bool signal_usable;
    bool eps_registered;
    bool gsm_registered;
    bool gprs_registered;
    bool packet_attached;
    bool operator_selected;
    bool pdp_active;
    int rssi;
    int ber;
    int eps_stat;
    int gsm_stat;
    int gprs_stat;
} ml307r_diag_t;

static bool response_has_ok(const char *response)
{
    return strstr(response, "\r\nOK\r\n") != NULL || strstr(response, "\nOK\r\n") != NULL || strstr(response, "OK\r\n") != NULL;
}

static bool response_has_error(const char *response)
{
    return strstr(response, "ERROR") != NULL || strstr(response, "+CME ERROR") != NULL || strstr(response, "+CMS ERROR") != NULL;
}

static bool registration_stat_is_registered(int stat)
{
    return stat == 1 || stat == 5;
}

static bool parse_csq(const char *response, int *rssi, int *ber)
{
    const char *line = strstr(response, "+CSQ");
    if (line == NULL) {
        return false;
    }

    if (sscanf(line, "+CSQ: \"%d\",\"%d\"", rssi, ber) == 2) {
        return true;
    }
    return sscanf(line, "+CSQ: %d,%d", rssi, ber) == 2;
}

static bool parse_registration_stat(const char *response, const char *prefix, int *stat)
{
    const char *line = strstr(response, prefix);
    if (line == NULL) {
        return false;
    }

    if (sscanf(line, "%*[^:]: %*d,%d", stat) == 1) {
        return true;
    }
    if (sscanf(line, "%*[^:]:%*d,%d", stat) == 1) {
        return true;
    }
    return sscanf(line, "%*[^=]=\"%d\"", stat) == 1;
}

static bool response_contains(const char *response, const char *needle)
{
    return strstr(response, needle) != NULL;
}

#ifndef ML307R_DIAG_TEST
static esp_err_t send_at_command(const at_command_t *command, char *response, size_t response_size)
{
    if (command == NULL || response == NULL || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    response[0] = '\0';
    (void)uart_flush_input(BOARD_ML307R_UART_NUM);
    ESP_LOGI(TAG, "> %s", command->name);

    ESP_RETURN_ON_ERROR(
        uart_write_bytes(BOARD_ML307R_UART_NUM, command->command, strlen(command->command)) < 0 ? ESP_FAIL : ESP_OK,
        TAG,
        "write AT failed");

    int total_len = 0;
    const TickType_t deadline_ticks = xTaskGetTickCount() + pdMS_TO_TICKS(command->timeout_ms);

    while (xTaskGetTickCount() < deadline_ticks && total_len < (int)response_size - 1) {
        const int len = uart_read_bytes(
            BOARD_ML307R_UART_NUM,
            (uint8_t *)response + total_len,
            response_size - 1 - total_len,
            pdMS_TO_TICKS(100));
        if (len > 0) {
            total_len += len;
            response[total_len] = '\0';
            if (response_has_ok(response) || response_has_error(response)) {
                break;
            }
        }
    }

    if (total_len == 0) {
        ESP_LOGW(TAG, "%s: no response after %d ms", command->name, command->timeout_ms);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "%s response:%s", command->name, response);
    return response_has_ok(response) ? ESP_OK : ESP_FAIL;
}

esp_err_t ml307r_service_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = BOARD_ML307R_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(
        BOARD_ML307R_UART_NUM,
        BOARD_ML307R_RX_BUF_SIZE,
        BOARD_ML307R_TX_BUF_SIZE,
        0,
        NULL,
        0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ESP_RETURN_ON_ERROR(uart_param_config(BOARD_ML307R_UART_NUM, &uart_config), TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(
            BOARD_ML307R_UART_NUM,
            BOARD_ML307R_TX_PIN,
            BOARD_ML307R_RX_PIN,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE),
        TAG,
        "uart_set_pin failed");

    (void)uart_flush_input(BOARD_ML307R_UART_NUM);
    return ESP_OK;
}

esp_err_t ml307r_service_probe(void)
{
    char response[128];
    const at_command_t command = {
        .name = "AT",
        .command = "AT\r\n",
        .timeout_ms = BOARD_ML307R_AT_TIMEOUT_MS,
    };

    return send_at_command(&command, response, sizeof(response));
}

esp_err_t ml307r_service_check_network(void)
{
    char response[512];
    esp_err_t result = ESP_OK;
    ml307r_diag_t diag = {
        .rssi = 99,
        .ber = 99,
        .eps_stat = -1,
        .gsm_stat = -1,
        .gprs_stat = -1,
    };
    const at_command_t commands[] = {
        {.name = "BASIC_AT", .command = "AT\r\n", .timeout_ms = 2000},
        {.name = "ECHO_OFF", .command = "ATE0\r\n", .timeout_ms = 2000},
        {.name = "MODULE_INFO", .command = "ATI\r\n", .timeout_ms = 2000},
        {.name = "FIRMWARE", .command = "AT+CGMR\r\n", .timeout_ms = 2000},
        {.name = "IMEI", .command = "AT+CGSN\r\n", .timeout_ms = 2000},
        {.name = "SIM_READY", .command = "AT+CPIN?\r\n", .timeout_ms = 3000},
        {.name = "ICCID", .command = "AT+CCID\r\n", .timeout_ms = 3000},
        {.name = "FUNCTION_LEVEL", .command = "AT+CFUN?\r\n", .timeout_ms = 3000},
        {.name = "SIGNAL", .command = "AT+CSQ\r\n", .timeout_ms = 3000},
        {.name = "EPS_REG_VERBOSE", .command = "AT+CEREG=2\r\n", .timeout_ms = 3000},
        {.name = "EPS_REG", .command = "AT+CEREG?\r\n", .timeout_ms = 5000},
        {.name = "GSM_REG", .command = "AT+CREG?\r\n", .timeout_ms = 5000},
        {.name = "GPRS_REG", .command = "AT+CGREG?\r\n", .timeout_ms = 5000},
        {.name = "PS_ATTACH", .command = "AT+CGATT?\r\n", .timeout_ms = 5000},
        {.name = "OPERATOR", .command = "AT+COPS?\r\n", .timeout_ms = 8000},
        {.name = "PDP_CONTEXT", .command = "AT+CGDCONT?\r\n", .timeout_ms = 5000},
        {.name = "PDP_ACTIVE", .command = "AT+CGACT?\r\n", .timeout_ms = 5000},
        {.name = "PDP_ADDRESS", .command = "AT+CGPADDR=1\r\n", .timeout_ms = 5000},
    };

    ESP_LOGI(TAG, "network diagnostic start");
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        const esp_err_t ret = send_at_command(&commands[i], response, sizeof(response));
        if (ret != ESP_OK) {
            result = ret;
        }

        if (strcmp(commands[i].name, "BASIC_AT") == 0) {
            diag.module_ok = ret == ESP_OK;
        } else if (strcmp(commands[i].name, "SIM_READY") == 0) {
            diag.sim_ready = response_contains(response, "+CPIN: READY");
        } else if (strcmp(commands[i].name, "SIGNAL") == 0 && parse_csq(response, &diag.rssi, &diag.ber)) {
            diag.signal_known = diag.rssi != 99;
            diag.signal_usable = diag.rssi >= 10 && diag.rssi <= 31;
        } else if (strcmp(commands[i].name, "EPS_REG") == 0 && parse_registration_stat(response, "+CEREG", &diag.eps_stat)) {
            diag.eps_registered = registration_stat_is_registered(diag.eps_stat);
        } else if (strcmp(commands[i].name, "GSM_REG") == 0 && parse_registration_stat(response, "+CREG", &diag.gsm_stat)) {
            diag.gsm_registered = registration_stat_is_registered(diag.gsm_stat);
        } else if (strcmp(commands[i].name, "GPRS_REG") == 0 && parse_registration_stat(response, "+CGREG", &diag.gprs_stat)) {
            diag.gprs_registered = registration_stat_is_registered(diag.gprs_stat);
        } else if (strcmp(commands[i].name, "PS_ATTACH") == 0) {
            diag.packet_attached = response_contains(response, "+CGATT: 1") || response_contains(response, "+CGATT=1");
        } else if (strcmp(commands[i].name, "OPERATOR") == 0) {
            diag.operator_selected = ret == ESP_OK && response_contains(response, "+COPS:");
        } else if (strcmp(commands[i].name, "PDP_ACTIVE") == 0) {
            diag.pdp_active = response_contains(response, "+CGACT: 1,1") || response_contains(response, "+CGACT=1,1");
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }

    ESP_LOGI(TAG,
        "summary: module=%s sim=%s signal=%s rssi=%d ber=%d eps_stat=%d gsm_stat=%d gprs_stat=%d attach=%s operator=%s pdp=%s",
        diag.module_ok ? "OK" : "FAIL",
        diag.sim_ready ? "READY" : "NOT_READY",
        diag.signal_usable ? "USABLE" : (diag.signal_known ? "WEAK" : "UNKNOWN"),
        diag.rssi,
        diag.ber,
        diag.eps_stat,
        diag.gsm_stat,
        diag.gprs_stat,
        diag.packet_attached ? "YES" : "NO",
        diag.operator_selected ? "YES" : "NO",
        diag.pdp_active ? "ACTIVE" : "INACTIVE");

    if (!diag.module_ok || !diag.sim_ready || !diag.signal_usable ||
        (!diag.eps_registered && !diag.gsm_registered && !diag.gprs_registered) || !diag.packet_attached) {
        result = ESP_FAIL;
    }

    ESP_LOGI(TAG, "network diagnostic %s", result == ESP_OK ? "PASS" : "FAIL");
    return result;
}
#endif
