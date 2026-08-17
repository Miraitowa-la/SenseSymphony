#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ai_uart_comm.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SLAVE_UART_PORT          UART_NUM_1
#define SLAVE_UART_TX_GPIO       GPIO_NUM_4
#define SLAVE_UART_RX_GPIO       GPIO_NUM_5
#define SLAVE_UART_BAUD          115200
#define MODE_SWITCH_GPIO         GPIO_NUM_21
#define MODE_SWITCH_DEBOUNCE_TICKS pdMS_TO_TICKS(250)
#define STATUS_QUERY_TICKS       pdMS_TO_TICKS(1000)

static const char *TAG = "ai_master";
static volatile bool s_mode_switch_event;

static void IRAM_ATTR mode_switch_isr(void *arg)
{
    (void)arg;
    s_mode_switch_event = true;
}

static void mode_switch_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << MODE_SWITCH_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(MODE_SWITCH_GPIO, mode_switch_isr, NULL));
    ESP_LOGI(TAG, "mode switch gpio: GPIO%d, short to 3.3V to switch slave mode", MODE_SWITCH_GPIO);
}

static bool mode_switch_clicked(void)
{
    static TickType_t last_click_tick;

    if (!s_mode_switch_event) {
        return false;
    }
    s_mode_switch_event = false;

    TickType_t now = xTaskGetTickCount();
    if (now - last_click_tick < MODE_SWITCH_DEBOUNCE_TICKS || gpio_get_level(MODE_SWITCH_GPIO) != 1) {
        return false;
    }

    last_click_tick = now;
    return true;
}

static void log_snapshot(const ai_uart_snapshot_t *snapshot)
{
    ESP_LOGI(TAG, "seq=%lu mode=%s count=%u",
             (unsigned long)snapshot->sequence,
             ai_uart_mode_name(snapshot->mode),
             (unsigned)snapshot->object_count);
    for (size_t i = 0; i < snapshot->object_count; i++) {
        const ai_uart_object_t *object = &snapshot->objects[i];
        ESP_LOGI(TAG, "obj[%u]: x10=%u y10=%u action=%d detect=%u action_conf=%u",
                 (unsigned)i,
                 object->x10,
                 object->y10,
                 object->action,
                 object->detect_confidence,
                 object->action_confidence);
    }
}

void app_main(void)
{
    const ai_uart_config_t uart_config = {
        .port = SLAVE_UART_PORT,
        .tx_gpio = SLAVE_UART_TX_GPIO,
        .rx_gpio = SLAVE_UART_RX_GPIO,
        .baud_rate = SLAVE_UART_BAUD,
    };
    ESP_ERROR_CHECK(ai_uart_init(&uart_config));
    mode_switch_init();

    TickType_t last_query_tick = 0;
    ESP_LOGI(TAG, "query slave every %d ms", (int)pdTICKS_TO_MS(STATUS_QUERY_TICKS));

    while (1) {
        if (mode_switch_clicked()) {
            ESP_ERROR_CHECK(ai_uart_master_send_next());
            ESP_LOGI(TAG, "send: NEXT");
        }

        TickType_t now = xTaskGetTickCount();
        if (now - last_query_tick >= STATUS_QUERY_TICKS) {
            last_query_tick = now;
            ESP_ERROR_CHECK(ai_uart_master_send_get());
        }

        ai_uart_snapshot_t snapshot;
        while (ai_uart_master_poll_snapshot(&snapshot)) {
            log_snapshot(&snapshot);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
