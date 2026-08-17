#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "ai_uart_comm.h"
#include "bsp_camera_ov5647.h"
#include "bsp_fps_overlay.h"
#include "bsp_lcd_jd9365_10_1.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "face_detect_middleware.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hand_detect_middleware.h"

#define CAMERA_PREVIEW_X         0
#define CAMERA_PREVIEW_Y         ((BSP_LCD_V_RES - BSP_CAMERA_OV5647_V_RES) / 2)
#define LCD_CLEAR_LINES          40
#define DETECT_INTERVAL          5
#define MAX_DETECT_BOXES         4
#define FACE_COORD_ACTIVE_X_PERCENT 60
#define FACE_COORD_ACTIVE_Y_PERCENT 60
#define EXPR_COORD_ACTIVE_X_PERCENT 60
#define EXPR_COORD_ACTIVE_Y_PERCENT 60
#define HAND_COORD_ACTIVE_X_PERCENT 60
#define HAND_COORD_ACTIVE_Y_PERCENT 60
#define GEST_COORD_ACTIVE_X_PERCENT 60
#define GEST_COORD_ACTIVE_Y_PERCENT 60
#define COORDINATE_OUTPUT_INVERT_X  true
#define COORDINATE_OUTPUT_INVERT_Y  false
#define MODE_SWITCH_GPIO         GPIO_NUM_21
#define MODE_SWITCH_DEBOUNCE_TICKS pdMS_TO_TICKS(250)
#define HOST_UART_PORT           UART_NUM_2
#define HOST_UART_TX_GPIO        GPIO_NUM_4
#define HOST_UART_RX_GPIO        GPIO_NUM_5
#define HOST_UART_BAUD           115200
#define RGB565_BLACK             0x0000
/* Set either axis to true before building to mirror that display axis. */
#define DISPLAY_MIRROR_X         false
#define DISPLAY_MIRROR_Y         true
#define FPS_OVERLAY_ENABLED      false

static const char *TAG = "camera_preview";
static volatile bool s_mode_switch_event;

static uint16_t map_position_percent10(uint16_t position, uint8_t active_percent, bool invert_axis);

static void coordinate_mapping_self_check(uint8_t active_percent, bool invert_axis)
{
    const uint16_t margin = (100 - active_percent) * 5;
    const uint16_t low = invert_axis ? 1000 : 0;
    const uint16_t high = invert_axis ? 0 : 1000;

    assert(map_position_percent10(0, active_percent, invert_axis) == low);
    assert(map_position_percent10(margin, active_percent, invert_axis) == low);
    assert(map_position_percent10(500, active_percent, invert_axis) == 500);
    assert(map_position_percent10(1000 - margin, active_percent, invert_axis) == high);
    assert(map_position_percent10(1000, active_percent, invert_axis) == high);
}

static void IRAM_ATTR mode_switch_isr(void *arg)
{
    (void)arg;
    s_mode_switch_event = true;
}

static void clear_lcd(uint16_t color)
{
    const size_t line_buf_pixels = BSP_LCD_H_RES * LCD_CLEAR_LINES;
    uint16_t *line_buf = heap_caps_malloc(line_buf_pixels * sizeof(uint16_t),
                                          MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!line_buf) {
        ESP_LOGE(TAG, "No memory for LCD clear buffer");
        abort();
    }

    for (size_t i = 0; i < line_buf_pixels; i++) {
        line_buf[i] = color;
    }

    for (int y = 0; y < BSP_LCD_V_RES; y += LCD_CLEAR_LINES) {
        int lines = BSP_LCD_V_RES - y;
        if (lines > LCD_CLEAR_LINES) {
            lines = LCD_CLEAR_LINES;
        }

        ESP_ERROR_CHECK(bsp_lcd_draw_bitmap(0, y, BSP_LCD_H_RES, y + lines, line_buf));
        ESP_ERROR_CHECK(bsp_lcd_wait_flush_done(portMAX_DELAY));
    }

    heap_caps_free(line_buf);
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
    ESP_LOGI(TAG, "mode switch gpio: GPIO%d, short to 3.3V to switch mode", MODE_SWITCH_GPIO);
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

static uint16_t scale_confidence(float score)
{
    if (score <= 0.0f) {
        return 0;
    }
    if (score >= 1.0f) {
        return 1000;
    }
    return (uint16_t)(score * 1000.0f + 0.5f);
}

static uint16_t center_percent10(int a, int b, int size)
{
    if (size <= 0) {
        return 0;
    }
    int value = ((a + b) * 500) / size;
    if (value < 0) {
        return 0;
    }
    if (value > 1000) {
        return 1000;
    }
    return (uint16_t)value;
}

static uint16_t map_position_percent10(uint16_t position, uint8_t active_percent, bool invert_axis)
{
    if (active_percent == 0 || active_percent > 100) {
        return invert_axis ? 1000 - position : position;
    }

    const int active_per_mille = active_percent * 10;
    const int margin = (1000 - active_per_mille) / 2;
    int value = (((int)position - margin) * 1000 + active_per_mille / 2) / active_per_mille;
    uint16_t mapped = value < 0 ? 0 : value > 1000 ? 1000 : (uint16_t)value;
    return invert_axis ? 1000 - mapped : mapped;
}

static int16_t expression_code(face_expression_t expression)
{
    return (int16_t)expression;
}

static void fill_snapshot(ai_uart_snapshot_t *snapshot,
                          ai_uart_mode_t mode,
                          uint32_t sequence,
                          int frame_width,
                          int frame_height,
                          const face_detect_result_t *face_result,
                          const hand_detect_result_t *hand_result)
{
    snapshot->sequence = sequence;
    snapshot->mode = mode;
    snapshot->object_count = 0;

    if (mode == AI_UART_MODE_FACE || mode == AI_UART_MODE_EXPR) {
        const uint8_t active_x = mode == AI_UART_MODE_FACE ? FACE_COORD_ACTIVE_X_PERCENT :
                                                            EXPR_COORD_ACTIVE_X_PERCENT;
        const uint8_t active_y = mode == AI_UART_MODE_FACE ? FACE_COORD_ACTIVE_Y_PERCENT :
                                                            EXPR_COORD_ACTIVE_Y_PERCENT;
        for (size_t i = 0; i < face_result->count && snapshot->object_count < AI_UART_MAX_OBJECTS; i++) {
            const face_detect_box_t *box = &face_result->boxes[i];
            ai_uart_object_t *object = &snapshot->objects[snapshot->object_count++];
            object->x10 = map_position_percent10(center_percent10(box->x1, box->x2, frame_width), active_x,
                                                  COORDINATE_OUTPUT_INVERT_X);
            object->y10 = map_position_percent10(center_percent10(box->y1, box->y2, frame_height), active_y,
                                                  COORDINATE_OUTPUT_INVERT_Y);
            object->action = mode == AI_UART_MODE_EXPR ? expression_code(box->expression) : -1;
            object->detect_confidence = mode == AI_UART_MODE_EXPR ? scale_confidence(box->score) : 0;
            object->action_confidence = 0;
        }
        return;
    }

    const uint8_t active_x = mode == AI_UART_MODE_HAND ? HAND_COORD_ACTIVE_X_PERCENT :
                                                        GEST_COORD_ACTIVE_X_PERCENT;
    const uint8_t active_y = mode == AI_UART_MODE_HAND ? HAND_COORD_ACTIVE_Y_PERCENT :
                                                        GEST_COORD_ACTIVE_Y_PERCENT;
    for (size_t i = 0; i < hand_result->count && snapshot->object_count < AI_UART_MAX_OBJECTS; i++) {
        const hand_detect_box_t *box = &hand_result->boxes[i];
        ai_uart_object_t *object = &snapshot->objects[snapshot->object_count++];
        object->x10 = map_position_percent10(center_percent10(box->x1, box->x2, frame_width), active_x,
                                              COORDINATE_OUTPUT_INVERT_X);
        object->y10 = map_position_percent10(center_percent10(box->y1, box->y2, frame_height), active_y,
                                              COORDINATE_OUTPUT_INVERT_Y);
        object->action = mode == AI_UART_MODE_GEST ? ai_uart_gesture_code(box->gesture) : -1;
        object->detect_confidence = mode == AI_UART_MODE_GEST ? scale_confidence(box->score) : 0;
        object->action_confidence = mode == AI_UART_MODE_GEST ? scale_confidence(box->gesture_score) : 0;
    }
}

void app_main(void)
{
    coordinate_mapping_self_check(FACE_COORD_ACTIVE_X_PERCENT, COORDINATE_OUTPUT_INVERT_X);
    coordinate_mapping_self_check(FACE_COORD_ACTIVE_Y_PERCENT, COORDINATE_OUTPUT_INVERT_Y);
    coordinate_mapping_self_check(EXPR_COORD_ACTIVE_X_PERCENT, COORDINATE_OUTPUT_INVERT_X);
    coordinate_mapping_self_check(EXPR_COORD_ACTIVE_Y_PERCENT, COORDINATE_OUTPUT_INVERT_Y);
    coordinate_mapping_self_check(HAND_COORD_ACTIVE_X_PERCENT, COORDINATE_OUTPUT_INVERT_X);
    coordinate_mapping_self_check(HAND_COORD_ACTIVE_Y_PERCENT, COORDINATE_OUTPUT_INVERT_Y);
    coordinate_mapping_self_check(GEST_COORD_ACTIVE_X_PERCENT, COORDINATE_OUTPUT_INVERT_X);
    coordinate_mapping_self_check(GEST_COORD_ACTIVE_Y_PERCENT, COORDINATE_OUTPUT_INVERT_Y);
    ESP_ERROR_CHECK(bsp_lcd_init());
    ESP_ERROR_CHECK(bsp_lcd_mirror(DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
    clear_lcd(RGB565_BLACK);
    mode_switch_init();

    const ai_uart_config_t uart_config = {
        .port = HOST_UART_PORT,
        .tx_gpio = HOST_UART_TX_GPIO,
        .rx_gpio = HOST_UART_RX_GPIO,
        .baud_rate = HOST_UART_BAUD,
    };
    ESP_ERROR_CHECK(ai_uart_init(&uart_config));

    bsp_fps_overlay_config_t fps_config = {
        .area_x = CAMERA_PREVIEW_X,
        .area_y = CAMERA_PREVIEW_Y,
        .area_w = BSP_CAMERA_OV5647_H_RES,
        .area_h = BSP_CAMERA_OV5647_V_RES,
        .margin_x = 12,
        .margin_y = 12,
        .scale = 3,
        .bg_color = 0x0000,
        .label_color = 0xFFE0,
        .value_color = 0xFFFF,
        .align = BSP_FPS_OVERLAY_ALIGN_BOTTOM_RIGHT,
        .enabled = FPS_OVERLAY_ENABLED,
    };
    ESP_ERROR_CHECK(bsp_fps_overlay_init(&fps_config));

    ESP_ERROR_CHECK(bsp_camera_ov5647_init());

    face_detect_box_t face_boxes[MAX_DETECT_BOXES] = {0};
    face_detect_result_t face_result = {
        .boxes = face_boxes,
        .capacity = MAX_DETECT_BOXES,
        .count = 0,
    };
    hand_detect_box_t hand_boxes[MAX_DETECT_BOXES] = {0};
    hand_detect_result_t hand_result = {
        .boxes = hand_boxes,
        .capacity = MAX_DETECT_BOXES,
        .count = 0,
    };
    ai_uart_mode_t mode = AI_UART_MODE_FACE;
    ai_uart_snapshot_t snapshot = {0};
    uint32_t update_sequence = 0;
    uint32_t frame_index = 0;

    ESP_LOGI(TAG, "mode: %s", ai_uart_mode_name(mode));

    while (1) {
        ai_uart_command_t command;
        while (ai_uart_slave_poll_command(&command)) {
            if (command.type == AI_UART_CMD_GET) {
                fill_snapshot(&snapshot, mode, update_sequence, BSP_CAMERA_OV5647_H_RES, BSP_CAMERA_OV5647_V_RES,
                              &face_result, &hand_result);
                ESP_ERROR_CHECK(ai_uart_slave_send_snapshot(&snapshot));
            } else if (command.type == AI_UART_CMD_NEXT || command.type == AI_UART_CMD_SET_MODE) {
                mode = command.type == AI_UART_CMD_NEXT ? (ai_uart_mode_t)((mode + 1) % AI_UART_MODE_COUNT)
                                                        : command.mode;
                face_result.count = 0;
                hand_result.count = 0;
                update_sequence++;
                ESP_LOGI(TAG, "mode: %s", ai_uart_mode_name(mode));
                fill_snapshot(&snapshot, mode, update_sequence, BSP_CAMERA_OV5647_H_RES, BSP_CAMERA_OV5647_V_RES,
                              &face_result, &hand_result);
                ESP_ERROR_CHECK(ai_uart_slave_send_snapshot(&snapshot));
            }
        }

        if (mode_switch_clicked()) {
            mode = (ai_uart_mode_t)((mode + 1) % AI_UART_MODE_COUNT);
            face_result.count = 0;
            hand_result.count = 0;
            update_sequence++;
            ESP_LOGI(TAG, "mode: %s", ai_uart_mode_name(mode));
            fill_snapshot(&snapshot, mode, update_sequence, BSP_CAMERA_OV5647_H_RES, BSP_CAMERA_OV5647_V_RES,
                          &face_result, &hand_result);
            ESP_ERROR_CHECK(ai_uart_slave_send_snapshot(&snapshot));
        }

        bsp_camera_frame_t frame = {0};
        esp_err_t ret = bsp_camera_ov5647_get_frame(&frame, pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "wait camera frame timeout");
            continue;
        }

        bsp_fps_overlay_count_frame();

        if ((frame_index++ % DETECT_INTERVAL) == 0) {
            bool face_mode = mode == AI_UART_MODE_FACE || mode == AI_UART_MODE_EXPR;
            bool gesture_mode = mode == AI_UART_MODE_GEST;
            if (face_mode) {
                ret = face_detect_middleware_detect_rgb565((const uint16_t *)frame.buffer,
                                                           frame.width,
                                                           frame.height,
                                                           &face_result);
                hand_result.count = 0;
                if (ret == ESP_OK) {
                    for (size_t i = 0; i < face_result.count; i++) {
                        if (mode == AI_UART_MODE_EXPR) {
                            ESP_LOGI(TAG, "face[%u]: %s score %.2f",
                                     (unsigned)i,
                                     face_detect_middleware_expression_name(face_result.boxes[i].expression),
                                     face_result.boxes[i].score);
                        } else {
                            ESP_LOGI(TAG, "face[%u]: score %.2f", (unsigned)i, face_result.boxes[i].score);
                        }
                    }
                } else {
                    ESP_LOGW(TAG, "face detect failed: %s", esp_err_to_name(ret));
                    face_result.count = 0;
                }
            } else {
                ret = hand_detect_middleware_detect_rgb565_ex((const uint16_t *)frame.buffer,
                                                              frame.width,
                                                              frame.height,
                                                              gesture_mode,
                                                              &hand_result);
                face_result.count = 0;
                if (ret == ESP_OK) {
                    for (size_t i = 0; i < hand_result.count; i++) {
                        if (gesture_mode) {
                            ESP_LOGI(TAG, "hand[%u]: score %.2f gesture %s %.2f",
                                     (unsigned)i,
                                     hand_result.boxes[i].score,
                                     hand_result.boxes[i].gesture,
                                     hand_result.boxes[i].gesture_score);
                        } else {
                            ESP_LOGI(TAG, "hand[%u]: score %.2f", (unsigned)i, hand_result.boxes[i].score);
                        }
                    }
                } else {
                    ESP_LOGW(TAG, "hand detect failed: %s", esp_err_to_name(ret));
                    hand_result.count = 0;
                }
            }
            update_sequence++;
        }

        if (face_result.count > 0) {
            ESP_ERROR_CHECK(face_detect_middleware_draw_boxes_rgb565((uint16_t *)frame.buffer,
                                                                     frame.width,
                                                                     frame.height,
                                                                     frame.width,
                                                                     &face_result));
        }
        if (hand_result.count > 0) {
            ESP_ERROR_CHECK(hand_detect_middleware_draw_boxes_rgb565((uint16_t *)frame.buffer,
                                                                     frame.width,
                                                                     frame.height,
                                                                     frame.width,
                                                                     &hand_result));
        }

        ESP_ERROR_CHECK(bsp_fps_overlay_draw_to_rgb565((uint16_t *)frame.buffer,
                                                       frame.width,
                                                       frame.height,
                                                       frame.width,
                                                       CAMERA_PREVIEW_X,
                                                       CAMERA_PREVIEW_Y));

        ESP_ERROR_CHECK(bsp_lcd_draw_bitmap(CAMERA_PREVIEW_X, CAMERA_PREVIEW_Y,
                                            CAMERA_PREVIEW_X + BSP_CAMERA_OV5647_H_RES,
                                            CAMERA_PREVIEW_Y + BSP_CAMERA_OV5647_V_RES,
                                            frame.buffer));
        ESP_ERROR_CHECK(bsp_lcd_wait_flush_done(portMAX_DELAY));
        ESP_ERROR_CHECK(bsp_camera_ov5647_return_frame(frame.buffer));
    }
}
