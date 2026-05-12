#include "key_service.h"

#include "board_config.h"
#include "driver/gpio.h"

esp_err_t key_service_init(void)
{
    const uint64_t key_pin_mask = (1ULL << BOARD_KEY1_PIN) | (1ULL << BOARD_KEY2_PIN);
    const gpio_config_t config = {
        .pin_bit_mask = key_pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&config);
}

key_state_t key_service_read(void)
{
    const key_state_t state = {
        .key1_pressed = gpio_get_level(BOARD_KEY1_PIN) == BOARD_KEY_ACTIVE_LEVEL,
        .key2_pressed = gpio_get_level(BOARD_KEY2_PIN) == BOARD_KEY_ACTIVE_LEVEL,
    };

    return state;
}

bool key_service_is_pressed(key_id_t key_id)
{
    const key_state_t state = key_service_read();

    switch (key_id) {
        case KEY_ID_1:
            return state.key1_pressed;
        case KEY_ID_2:
            return state.key2_pressed;
        default:
            return false;
    }
}
