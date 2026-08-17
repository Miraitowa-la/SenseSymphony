#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsp_lcd_jd9365_10_1.h"
#include "ui/home_screen.h"
#include "uart/ai_uart_service.h"
#include "uart/app_uart_service.h"
#include "storage/mode1_history_service.h"
#include "storage/mode2_history_service.h"
#include "storage/mode2_user_song_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define LVGL_TASK_DELAY_MS   5
#define LVGL_TASK_STACK_SIZE (16 * 1024)
#define LVGL_TASK_PRIORITY   5

static const char *TAG = "lvgl_demo";

/*
 * LVGL 和 BSP LCD 都使用 RGB565。
 * 使用 MIPI-DPI 驱动管理的双整屏帧缓冲；LVGL 直接渲染，驱动在帧边界切换。
 */
static lv_display_t *s_lvgl_display;
static void *s_lvgl_frame_buffers[2];

/* 触摸输入状态由 BSP 的 GT9271 驱动提供，LVGL 指针设备只取第一个触点控制 UI。 */
static bool s_touch_ready;
static lv_point_t s_last_touch_point;

static uint32_t lvgl_tick_get_cb(void)
{
    /* LVGL 需要毫秒级单调时基，ESP timer 返回微秒，这里转换为 ms。 */
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /*
     * LVGL 默认的区域坐标是闭区间 [x1, x2] / [y1, y2]；
     * esp_lcd_panel_draw_bitmap() 使用右下角不包含的区间，因此 x2/y2 需要 +1。
     */
    ESP_ERROR_CHECK(bsp_lcd_draw_bitmap(area->x1, area->y1,
                                        area->x2 + 1, area->y2 + 1,
                                        px_map));
    lv_display_flush_ready(disp);
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    if (!s_touch_ready) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point = s_last_touch_point;
        return;
    }

    bsp_touch_data_t touch_data = {0};
    esp_err_t ret = bsp_touch_read_points(&touch_data);
    if (ret != ESP_OK) {
        /* 读触摸失败时保持上一次坐标并释放指针，避免 UI 误判为连续按下。 */
        data->state = LV_INDEV_STATE_RELEASED;
        data->point = s_last_touch_point;
        return;
    }

    if (touch_data.count > 0) {
        s_last_touch_point.x = touch_data.points[0].x;
        s_last_touch_point.y = touch_data.points[0].y;
        data->point = s_last_touch_point;
        data->state = LV_INDEV_STATE_PRESSED;

    } else {
        data->point = s_last_touch_point;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void init_lvgl_port(void)
{
    lv_init();
    lv_tick_set_cb(lvgl_tick_get_cb);

    const size_t lvgl_buf_size = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t);

    s_lvgl_display = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);
    if (!s_lvgl_display) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        abort();
    }

    lv_display_set_flush_cb(s_lvgl_display, lvgl_flush_cb);
    lv_display_set_buffers(s_lvgl_display, s_lvgl_frame_buffers[0], s_lvgl_frame_buffers[1],
                           lvgl_buf_size, LV_DISPLAY_RENDER_MODE_FULL);

    /* 注册触摸为 LVGL 指针输入设备，Slider/Button/Switch 会自动接收触摸事件。 */
    lv_indev_t *touch_indev = lv_indev_create();
    if (!touch_indev) {
        ESP_LOGE(TAG, "Failed to create LVGL input device");
        abort();
    }

    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    /*
     * LVGL 初始化、对象创建和 lv_timer_handler() 都放在同一个任务中执行。
     * 这样既能避免多任务同时访问 LVGL，也能给 LVGL 分配比 app_main 默认值更大的栈。
     */
    init_lvgl_port();
    home_screen_create();

    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_DELAY_MS));
    }
}

void app_main(void)
{
    /* 先初始化 LCD，再初始化触摸和 LVGL；LVGL flush 回调依赖 BSP LCD 已经可用。 */
    ESP_ERROR_CHECK(bsp_lcd_init());
    ESP_ERROR_CHECK(bsp_lcd_get_frame_buffers(&s_lvgl_frame_buffers[0], &s_lvgl_frame_buffers[1]));

    esp_err_t touch_ret = bsp_touch_init();
    if (touch_ret == ESP_OK) {
        s_touch_ready = true;
        ESP_LOGI(TAG, "Touch input ready");
    } else {
        ESP_LOGW(TAG, "Touch init failed: %s", esp_err_to_name(touch_ret));
    }

    ESP_ERROR_CHECK(ai_uart_service_start());
    (void)mode1_history_init();
    (void)mode2_history_init();
    (void)mode2_user_song_init();
    ESP_ERROR_CHECK(app_uart_service_start());

    BaseType_t task_ret = xTaskCreatePinnedToCore(lvgl_task,
                                                  "lvgl",
                                                  LVGL_TASK_STACK_SIZE,
                                                  NULL,
                                                  LVGL_TASK_PRIORITY,
                                                  NULL,
                                                  tskNO_AFFINITY);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        abort();
    }
}
