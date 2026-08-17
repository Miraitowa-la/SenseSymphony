#include "bsp_fps_overlay.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "bsp_lcd_jd9365_10_1.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define FPS_FONT_W                  5
#define FPS_FONT_H                  7
#define FPS_TEXT_MAX_LEN            15
#define FPS_TEXT_PAD_X              6
#define FPS_TEXT_PAD_Y              6
#define FPS_DEFAULT_SCALE           3
#define FPS_DEFAULT_MARGIN_X        12
#define FPS_DEFAULT_MARGIN_Y        12

#define RGB565_BLACK                0x0000
#define RGB565_WHITE                0xFFFF
#define RGB565_YELLOW               0xFFE0

static const char *TAG = "fps_overlay";

static bsp_fps_overlay_config_t s_config = {
    .area_x = 0,
    .area_y = 0,
    .area_w = BSP_LCD_H_RES,
    .area_h = BSP_LCD_V_RES,
    .margin_x = FPS_DEFAULT_MARGIN_X,
    .margin_y = FPS_DEFAULT_MARGIN_Y,
    .custom_x = 0,
    .custom_y = 0,
    .scale = FPS_DEFAULT_SCALE,
    .bg_color = RGB565_BLACK,
    .label_color = RGB565_YELLOW,
    .value_color = RGB565_WHITE,
    .align = BSP_FPS_OVERLAY_ALIGN_BOTTOM_RIGHT,
    .enabled = true,
};

static uint16_t *s_overlay_buf;
static int s_overlay_w;
static int s_overlay_h;
static uint32_t s_fps_x10;
static uint32_t s_frame_count;
static int64_t s_window_start_us;
static bool s_initialized;

static const uint8_t s_font_digits[10][FPS_FONT_H] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
};

static const uint8_t s_font_f[FPS_FONT_H] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t s_font_p[FPS_FONT_H] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t s_font_s[FPS_FONT_H] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
static const uint8_t s_font_colon[FPS_FONT_H] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
static const uint8_t s_font_dot[FPS_FONT_H] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};

static const uint8_t *get_glyph(char c)
{
    if (c >= '0' && c <= '9') {
        return s_font_digits[c - '0'];
    }

    switch (c) {
    case 'F':
        return s_font_f;
    case 'P':
        return s_font_p;
    case 'S':
        return s_font_s;
    case ':':
        return s_font_colon;
    case '.':
        return s_font_dot;
    default:
        return NULL;
    }
}

static esp_err_t rebuild_buffer(void)
{
    uint8_t scale = s_config.scale ? s_config.scale : FPS_DEFAULT_SCALE;
    int char_advance = (FPS_FONT_W + 1) * scale;
    int text_w = FPS_TEXT_MAX_LEN * char_advance;
    int text_h = FPS_FONT_H * scale;
    int new_w = text_w + FPS_TEXT_PAD_X * 2;
    int new_h = text_h + FPS_TEXT_PAD_Y * 2;

    if (new_w == s_overlay_w && new_h == s_overlay_h && s_overlay_buf) {
        return ESP_OK;
    }

    if (s_overlay_buf) {
        heap_caps_free(s_overlay_buf);
        s_overlay_buf = NULL;
    }

    s_overlay_w = new_w;
    s_overlay_h = new_h;
    s_overlay_buf = heap_caps_malloc(s_overlay_w * s_overlay_h * sizeof(uint16_t),
                                     MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_overlay_buf, ESP_ERR_NO_MEM, TAG, "allocate FPS overlay buffer failed");
    return ESP_OK;
}

static void fill_overlay(uint16_t color)
{
    for (int i = 0; i < s_overlay_w * s_overlay_h; i++) {
        s_overlay_buf[i] = color;
    }
}

static void draw_scaled_char(int x0, int y0, char c, uint16_t color)
{
    const uint8_t *glyph = get_glyph(c);
    uint8_t scale = s_config.scale ? s_config.scale : FPS_DEFAULT_SCALE;

    if (!glyph) {
        return;
    }

    for (int row = 0; row < FPS_FONT_H; row++) {
        for (int col = 0; col < FPS_FONT_W; col++) {
            if ((glyph[row] & (1 << (FPS_FONT_W - 1 - col))) == 0) {
                continue;
            }

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int x = x0 + col * scale + sx;
                    int y = y0 + row * scale + sy;
                    if (x >= 0 && x < s_overlay_w && y >= 0 && y < s_overlay_h) {
                        s_overlay_buf[y * s_overlay_w + x] = color;
                    }
                }
            }
        }
    }
}

static void get_draw_position(int *x, int *y)
{
    switch (s_config.align) {
    case BSP_FPS_OVERLAY_ALIGN_TOP_LEFT:
        *x = s_config.area_x + s_config.margin_x;
        *y = s_config.area_y + s_config.margin_y;
        break;
    case BSP_FPS_OVERLAY_ALIGN_TOP_RIGHT:
        *x = s_config.area_x + s_config.area_w - s_overlay_w - s_config.margin_x;
        *y = s_config.area_y + s_config.margin_y;
        break;
    case BSP_FPS_OVERLAY_ALIGN_BOTTOM_LEFT:
        *x = s_config.area_x + s_config.margin_x;
        *y = s_config.area_y + s_config.area_h - s_overlay_h - s_config.margin_y;
        break;
    case BSP_FPS_OVERLAY_ALIGN_CUSTOM:
        *x = s_config.custom_x;
        *y = s_config.custom_y;
        break;
    case BSP_FPS_OVERLAY_ALIGN_BOTTOM_RIGHT:
    default:
        *x = s_config.area_x + s_config.area_w - s_overlay_w - s_config.margin_x;
        *y = s_config.area_y + s_config.area_h - s_overlay_h - s_config.margin_y;
        break;
    }
}

static void render_overlay_text(void)
{
    char text[16] = {0};
    snprintf(text, sizeof(text), "FPS:%lu.%lu",
             (unsigned long)(s_fps_x10 / 10),
             (unsigned long)(s_fps_x10 % 10));

    fill_overlay(s_config.bg_color);

    uint8_t scale = s_config.scale ? s_config.scale : FPS_DEFAULT_SCALE;
    int char_advance = (FPS_FONT_W + 1) * scale;
    int text_w = (int)strlen(text) * char_advance;
    int x = s_overlay_w - text_w - FPS_TEXT_PAD_X;
    int y = (s_overlay_h - FPS_FONT_H * scale) / 2;

    for (size_t i = 0; i < strlen(text); i++) {
        draw_scaled_char(x + i * char_advance, y, text[i],
                         i < 4 ? s_config.label_color : s_config.value_color);
    }
}

esp_err_t bsp_fps_overlay_init(const bsp_fps_overlay_config_t *config)
{
    if (config) {
        s_config = *config;
        if (s_config.scale == 0) {
            s_config.scale = FPS_DEFAULT_SCALE;
        }
    }

    ESP_RETURN_ON_FALSE(s_config.area_w > 0 && s_config.area_h > 0, ESP_ERR_INVALID_ARG,
                        TAG, "invalid FPS overlay area");
    ESP_RETURN_ON_ERROR(rebuild_buffer(), TAG, "rebuild FPS overlay buffer failed");

    s_fps_x10 = 0;
    s_frame_count = 0;
    s_window_start_us = esp_timer_get_time();
    s_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_fps_overlay_deinit(void)
{
    if (s_overlay_buf) {
        heap_caps_free(s_overlay_buf);
        s_overlay_buf = NULL;
    }

    s_overlay_w = 0;
    s_overlay_h = 0;
    s_initialized = false;
    return ESP_OK;
}

void bsp_fps_overlay_set_enabled(bool enabled)
{
    s_config.enabled = enabled;
}

bool bsp_fps_overlay_is_enabled(void)
{
    return s_config.enabled;
}

esp_err_t bsp_fps_overlay_set_area(int x, int y, int w, int h)
{
    ESP_RETURN_ON_FALSE(w > 0 && h > 0, ESP_ERR_INVALID_ARG, TAG, "invalid FPS overlay area");

    s_config.area_x = x;
    s_config.area_y = y;
    s_config.area_w = w;
    s_config.area_h = h;
    return ESP_OK;
}

esp_err_t bsp_fps_overlay_set_position(bsp_fps_overlay_align_t align, int margin_x, int margin_y)
{
    ESP_RETURN_ON_FALSE(align >= BSP_FPS_OVERLAY_ALIGN_TOP_LEFT &&
                        align <= BSP_FPS_OVERLAY_ALIGN_CUSTOM,
                        ESP_ERR_INVALID_ARG, TAG, "invalid FPS overlay alignment");
    ESP_RETURN_ON_FALSE(margin_x >= 0 && margin_y >= 0, ESP_ERR_INVALID_ARG,
                        TAG, "invalid FPS overlay margin");

    s_config.align = align;
    s_config.margin_x = margin_x;
    s_config.margin_y = margin_y;
    return ESP_OK;
}

esp_err_t bsp_fps_overlay_set_custom_position(int x, int y)
{
    s_config.custom_x = x;
    s_config.custom_y = y;
    s_config.align = BSP_FPS_OVERLAY_ALIGN_CUSTOM;
    return ESP_OK;
}

void bsp_fps_overlay_count_frame(void)
{
    int64_t now_us = esp_timer_get_time();

    if (s_window_start_us == 0) {
        s_window_start_us = now_us;
    }

    s_frame_count++;
    int64_t elapsed_us = now_us - s_window_start_us;
    if (elapsed_us >= 1000 * 1000) {
        s_fps_x10 = (uint32_t)((uint64_t)s_frame_count * 10000000ULL / (uint64_t)elapsed_us);
        s_frame_count = 0;
        s_window_start_us = now_us;
    }
}

uint32_t bsp_fps_overlay_get_fps_x10(void)
{
    return s_fps_x10;
}

esp_err_t bsp_fps_overlay_draw(void)
{
    if (!s_config.enabled) {
        return ESP_OK;
    }

    if (!s_initialized) {
        ESP_RETURN_ON_ERROR(bsp_fps_overlay_init(NULL), TAG, "auto init FPS overlay failed");
    }

    render_overlay_text();

    int draw_x = 0;
    int draw_y = 0;
    get_draw_position(&draw_x, &draw_y);

    ESP_RETURN_ON_ERROR(bsp_lcd_draw_bitmap(draw_x, draw_y,
                                            draw_x + s_overlay_w,
                                            draw_y + s_overlay_h,
                                            s_overlay_buf),
                        TAG, "draw FPS overlay failed");
    return bsp_lcd_wait_flush_done(portMAX_DELAY);
}

esp_err_t bsp_fps_overlay_draw_to_rgb565(uint16_t *frame_buffer,
                                         int frame_w,
                                         int frame_h,
                                         int stride_pixels,
                                         int screen_x,
                                         int screen_y)
{
    if (!s_config.enabled) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(frame_buffer, ESP_ERR_INVALID_ARG, TAG, "frame buffer is NULL");
    ESP_RETURN_ON_FALSE(frame_w > 0 && frame_h > 0, ESP_ERR_INVALID_ARG, TAG, "invalid frame size");

    if (!s_initialized) {
        ESP_RETURN_ON_ERROR(bsp_fps_overlay_init(NULL), TAG, "auto init FPS overlay failed");
    }

    if (stride_pixels <= 0) {
        stride_pixels = frame_w;
    }
    ESP_RETURN_ON_FALSE(stride_pixels >= frame_w, ESP_ERR_INVALID_ARG, TAG, "invalid frame stride");

    render_overlay_text();

    int draw_x = 0;
    int draw_y = 0;
    get_draw_position(&draw_x, &draw_y);

    int local_x = draw_x - screen_x;
    int local_y = draw_y - screen_y;
    int src_x = 0;
    int src_y = 0;
    int copy_w = s_overlay_w;
    int copy_h = s_overlay_h;

    if (local_x < 0) {
        src_x = -local_x;
        copy_w += local_x;
        local_x = 0;
    }
    if (local_y < 0) {
        src_y = -local_y;
        copy_h += local_y;
        local_y = 0;
    }
    if (local_x + copy_w > frame_w) {
        copy_w = frame_w - local_x;
    }
    if (local_y + copy_h > frame_h) {
        copy_h = frame_h - local_y;
    }

    if (copy_w <= 0 || copy_h <= 0) {
        return ESP_OK;
    }

    for (int row = 0; row < copy_h; row++) {
        uint16_t *dst = frame_buffer + (local_y + row) * stride_pixels + local_x;
        const uint16_t *src = s_overlay_buf + (src_y + row) * s_overlay_w + src_x;
        memcpy(dst, src, copy_w * sizeof(uint16_t));
        ESP_RETURN_ON_ERROR(esp_cache_msync(dst,
                                            copy_w * sizeof(uint16_t),
                                            ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                                            ESP_CACHE_MSYNC_FLAG_UNALIGNED),
                            TAG, "sync FPS overlay row failed");
    }
    return ESP_OK;
}
