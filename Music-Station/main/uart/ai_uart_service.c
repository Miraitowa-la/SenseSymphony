#include "ai_uart_service.h"

#include "ai_uart_comm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AI_UART_PORT UART_NUM_2
#define AI_UART_TX_GPIO GPIO_NUM_23
#define AI_UART_RX_GPIO GPIO_NUM_22
#define AI_UART_BAUD_RATE 115200

static ai_uart_snapshot_t s_snapshot;
static bool s_has_snapshot;
static ai_uart_mode_t s_mode = AI_UART_MODE_GEST;
static uint32_t s_interval_ms = 100;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;

void ai_uart_service_select_mode(ai_uart_mode_t mode, uint32_t poll_interval_ms)
{
    if (mode >= AI_UART_MODE_COUNT || poll_interval_ms == 0) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    s_mode = mode;
    s_interval_ms = poll_interval_ms;
    /* A screen must not act on the previous capture mode while SET is in flight. */
    s_has_snapshot = false;
    portEXIT_CRITICAL(&s_lock);
}

bool ai_uart_service_get_snapshot(ai_uart_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    *snapshot = s_snapshot;
    bool has_snapshot = s_has_snapshot;
    portEXIT_CRITICAL(&s_lock);
    return has_snapshot;
}

static void ai_uart_task(void *arg)
{
    (void)arg;
    ai_uart_config_t config = {
        .port = AI_UART_PORT,
        .tx_gpio = AI_UART_TX_GPIO,
        .rx_gpio = AI_UART_RX_GPIO,
        .baud_rate = AI_UART_BAUD_RATE,
    };
    while (ai_uart_init(&config) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ai_uart_mode_t sent_mode = AI_UART_MODE_COUNT;
    uint32_t last_get_ms = 0;
    for (;;) {
        ai_uart_mode_t mode;
        uint32_t interval_ms;
        portENTER_CRITICAL(&s_lock);
        mode = s_mode;
        interval_ms = s_interval_ms;
        portEXIT_CRITICAL(&s_lock);

        if (mode != sent_mode && ai_uart_master_send_set_mode(mode) == ESP_OK) {
            sent_mode = mode;
            last_get_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        }
        uint32_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        if (now_ms - last_get_ms >= interval_ms) {
            (void)ai_uart_master_send_get();
            last_get_ms = now_ms;
        }
        ai_uart_snapshot_t incoming;
        while (ai_uart_master_poll_snapshot(&incoming)) {
            portENTER_CRITICAL(&s_lock);
            if (incoming.mode == s_mode) {
                s_snapshot = incoming;
                s_has_snapshot = true;
            }
            portEXIT_CRITICAL(&s_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t ai_uart_service_start(void)
{
    if (s_task != NULL) {
        return ESP_OK;
    }
    return xTaskCreate(ai_uart_task, "ai_uart", 4096, NULL, 4, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
