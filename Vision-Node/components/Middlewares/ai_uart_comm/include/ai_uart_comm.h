#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AI_UART_MAX_OBJECTS 4

typedef enum {
    AI_UART_MODE_FACE = 0,
    AI_UART_MODE_EXPR,
    AI_UART_MODE_HAND,
    AI_UART_MODE_GEST,
    AI_UART_MODE_COUNT,
} ai_uart_mode_t;

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

typedef struct {
    uint16_t x10;
    uint16_t y10;
    int16_t action;
    uint16_t detect_confidence;
    uint16_t action_confidence;
} ai_uart_object_t;

typedef struct {
    uint32_t sequence;
    ai_uart_mode_t mode;
    size_t object_count;
    ai_uart_object_t objects[AI_UART_MAX_OBJECTS];
} ai_uart_snapshot_t;

esp_err_t ai_uart_init(const ai_uart_config_t *config);
const char *ai_uart_mode_name(ai_uart_mode_t mode);
bool ai_uart_mode_from_name(const char *name, ai_uart_mode_t *mode);
int16_t ai_uart_gesture_code(const char *gesture);

esp_err_t ai_uart_master_send_get(void);
esp_err_t ai_uart_master_send_next(void);
esp_err_t ai_uart_master_send_set_mode(ai_uart_mode_t mode);
bool ai_uart_master_poll_snapshot(ai_uart_snapshot_t *snapshot);

bool ai_uart_slave_poll_command(ai_uart_command_t *command);
esp_err_t ai_uart_slave_send_snapshot(const ai_uart_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif
