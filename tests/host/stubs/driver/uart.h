#ifndef DRIVER_UART_H
#define DRIVER_UART_H

#include <stddef.h>
#include <stdint.h>

#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1

typedef struct {
    int baud_rate;
    int data_bits;
    int parity;
    int stop_bits;
    int flow_ctrl;
    int rx_flow_ctrl_thresh;
    int source_clk;
} uart_config_t;

static inline int uart_flush_input(int uart_num) { (void)uart_num; return 0; }
static inline int uart_write_bytes(int uart_num, const char *data, size_t size) { (void)uart_num; (void)data; return (int)size; }
static inline int uart_read_bytes(int uart_num, uint8_t *buf, size_t len, int ticks) { (void)uart_num; (void)buf; (void)len; (void)ticks; return 0; }
static inline int uart_driver_install(int uart_num, int rx, int tx, int queue, void *queue_handle, int flags) { (void)uart_num; (void)rx; (void)tx; (void)queue; (void)queue_handle; (void)flags; return 0; }
static inline int uart_param_config(int uart_num, const uart_config_t *config) { (void)uart_num; (void)config; return 0; }
static inline int uart_set_pin(int uart_num, int tx, int rx, int rts, int cts) { (void)uart_num; (void)tx; (void)rx; (void)rts; (void)cts; return 0; }

#endif
