#include "ai_uart_comm.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ai_uart_parser.h"

static const char *TAG = "ai_uart";
static uart_port_t s_uart_port = UART_NUM_1;
static ai_uart_parser_t s_master_parser;

static const char *s_mode_names[AI_UART_MODE_COUNT] = {"FACE", "EXPR", "HAND", "GEST"};

esp_err_t ai_uart_init(const ai_uart_config_t *config)
{
    if (!config || config->port < UART_NUM_0 || config->port >= UART_NUM_MAX ||
        config->tx_gpio < 0 || config->rx_gpio < 0 || config->baud_rate <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_uart_port = config->port;
    const uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(s_uart_port, 2048, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = uart_param_config(s_uart_port, &uart_config);
    if (ret != ESP_OK) {
        uart_driver_delete(s_uart_port);
        return ret;
    }
    ret = uart_set_pin(s_uart_port, config->tx_gpio, config->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        uart_driver_delete(s_uart_port);
        return ret;
    }

    ESP_LOGI(TAG, "UART%d TX GPIO%d RX GPIO%d %d baud",
             s_uart_port, config->tx_gpio, config->rx_gpio, config->baud_rate);
    ai_uart_parser_init(&s_master_parser);
    return ESP_OK;
}

const char *ai_uart_mode_name(ai_uart_mode_t mode)
{
    return mode >= AI_UART_MODE_FACE && mode < AI_UART_MODE_COUNT ?
           s_mode_names[mode] : "UNKNOWN";
}

bool ai_uart_mode_from_name(const char *name, ai_uart_mode_t *mode)
{
    if (!name || !mode) {
        return false;
    }
    for (int i = 0; i < AI_UART_MODE_COUNT; i++) {
        if (strcmp(name, s_mode_names[i]) == 0) {
            *mode = (ai_uart_mode_t)i;
            return true;
        }
    }
    return false;
}

int16_t ai_uart_gesture_code(const char *gesture)
{
    static const char *names[] = {
        "one", "two", "three", "four", "five", "like",
        "ok", "no_gesture", "call", "dislike", "no_hand",
    };
    if (!gesture) {
        return -1;
    }
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(gesture, names[i]) == 0) {
            return (int16_t)i;
        }
    }
    return -1;
}

static esp_err_t write_line(const char *line)
{
    int len = (int)strlen(line);
    return uart_write_bytes(s_uart_port, line, len) == len ? ESP_OK : ESP_FAIL;
}

esp_err_t ai_uart_master_send_get(void)
{
    return write_line("GET\n");
}

esp_err_t ai_uart_master_send_next(void)
{
    return write_line("NEXT\n");
}

esp_err_t ai_uart_master_send_set_mode(ai_uart_mode_t mode)
{
    if (mode < AI_UART_MODE_FACE || mode >= AI_UART_MODE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    char line[16];
    snprintf(line, sizeof(line), "SET,%d\n", (int)mode);
    return write_line(line);
}

static bool read_line(char *line, size_t line_size)
{
    static char buffer[96];
    static size_t len;
    uint8_t byte;

    while (uart_read_bytes(s_uart_port, &byte, 1, 0) == 1) {
        if (byte == '\r') {
            continue;
        }
        if (byte != '\n') {
            if (len < sizeof(buffer) - 1) {
                buffer[len++] = (char)byte;
            }
            continue;
        }

        buffer[len] = '\0';
        strncpy(line, buffer, line_size - 1);
        line[line_size - 1] = '\0';
        len = 0;
        return true;
    }
    return false;
}

bool ai_uart_slave_poll_command(ai_uart_command_t *command)
{
    char line[96];
    int mode;

    if (!command || !read_line(line, sizeof(line))) {
        return false;
    }

    command->type = AI_UART_CMD_NONE;
    if (strcmp(line, "GET") == 0) {
        command->type = AI_UART_CMD_GET;
    } else if (strcmp(line, "NEXT") == 0) {
        command->type = AI_UART_CMD_NEXT;
    } else if (sscanf(line, "SET,%d", &mode) == 1 && mode >= 0 && mode < AI_UART_MODE_COUNT) {
        command->type = AI_UART_CMD_SET_MODE;
        command->mode = (ai_uart_mode_t)mode;
    } else if (strncmp(line, "SET ", 4) == 0 && ai_uart_mode_from_name(line + 4, &command->mode)) {
        command->type = AI_UART_CMD_SET_MODE;
    }

    return command->type != AI_UART_CMD_NONE;
}

esp_err_t ai_uart_slave_send_snapshot(const ai_uart_snapshot_t *snapshot)
{
    if (!snapshot || snapshot->object_count > AI_UART_MAX_OBJECTS) {
        return ESP_ERR_INVALID_ARG;
    }

    char line[96];
    snprintf(line, sizeof(line), "R,%lu,%d,%u\n",
             (unsigned long)snapshot->sequence,
             (int)snapshot->mode,
             (unsigned)snapshot->object_count);
    ESP_RETURN_ON_ERROR(write_line(line), TAG, "write snapshot header failed");

    for (size_t i = 0; i < snapshot->object_count; i++) {
        const ai_uart_object_t *object = &snapshot->objects[i];
        snprintf(line, sizeof(line), "O,%u,%u,%d,%u,%u\n",
                 object->x10,
                 object->y10,
                 object->action,
                 object->detect_confidence,
                 object->action_confidence);
        ESP_RETURN_ON_ERROR(write_line(line), TAG, "write snapshot object failed");
    }
    return ESP_OK;
}

bool ai_uart_master_poll_snapshot(ai_uart_snapshot_t *snapshot)
{
    char line[96];
    const uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

    if (!snapshot) {
        return false;
    }
    (void)ai_uart_parser_expire(&s_master_parser, now_ms);
    if (!read_line(line, sizeof(line))) {
        return false;
    }

    return ai_uart_parser_feed_line(&s_master_parser, line, now_ms, snapshot) ==
           AI_UART_PARSE_SNAPSHOT;
}

void ai_uart_get_stats(ai_uart_stats_t *stats)
{
    ai_uart_parser_get_stats(&s_master_parser, stats);
}
