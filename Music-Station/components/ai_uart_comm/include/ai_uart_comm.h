#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "ai_uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AI_UART_CMD_NONE = 0,
    AI_UART_CMD_GET,
    AI_UART_CMD_NEXT,
    AI_UART_CMD_SET_MODE,
} ai_uart_command_type_t;

typedef struct {
    uart_port_t port;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    int baud_rate;
} ai_uart_config_t;

typedef struct {
    ai_uart_command_type_t type;
    ai_uart_mode_t mode;
} ai_uart_command_t;

esp_err_t ai_uart_init(const ai_uart_config_t *config);
const char *ai_uart_mode_name(ai_uart_mode_t mode);
bool ai_uart_mode_from_name(const char *name, ai_uart_mode_t *mode);
int16_t ai_uart_gesture_code(const char *gesture);

esp_err_t ai_uart_master_send_get(void);
esp_err_t ai_uart_master_send_next(void);
esp_err_t ai_uart_master_send_set_mode(ai_uart_mode_t mode);
bool ai_uart_master_poll_snapshot(ai_uart_snapshot_t *snapshot);
void ai_uart_get_stats(ai_uart_stats_t *stats);

bool ai_uart_slave_poll_command(ai_uart_command_t *command);
esp_err_t ai_uart_slave_send_snapshot(const ai_uart_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif
