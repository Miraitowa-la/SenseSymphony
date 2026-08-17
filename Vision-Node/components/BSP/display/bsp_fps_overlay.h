#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsp_fps_overlay.h
 * @brief RGB565 FPS 叠加显示工具。
 *
 * 该模块负责统计帧率并在指定画面区域上绘制 FPS 文本。它只依赖 LCD 绘图接口，
 * 不依赖摄像头分辨率，因此可用于 800x640、800x1280 或其它 RGB565 画面。
 */

typedef enum {
    BSP_FPS_OVERLAY_ALIGN_TOP_LEFT = 0,
    BSP_FPS_OVERLAY_ALIGN_TOP_RIGHT,
    BSP_FPS_OVERLAY_ALIGN_BOTTOM_LEFT,
    BSP_FPS_OVERLAY_ALIGN_BOTTOM_RIGHT,
    BSP_FPS_OVERLAY_ALIGN_CUSTOM,
} bsp_fps_overlay_align_t;

typedef struct {
    int area_x;                       /*!< 画面区域左上角 X 坐标 */
    int area_y;                       /*!< 画面区域左上角 Y 坐标 */
    int area_w;                       /*!< 画面区域宽度 */
    int area_h;                       /*!< 画面区域高度 */
    int margin_x;                     /*!< 非自定义位置时的水平边距 */
    int margin_y;                     /*!< 非自定义位置时的垂直边距 */
    int custom_x;                     /*!< BSP_FPS_OVERLAY_ALIGN_CUSTOM 使用的绝对 X 坐标 */
    int custom_y;                     /*!< BSP_FPS_OVERLAY_ALIGN_CUSTOM 使用的绝对 Y 坐标 */
    uint8_t scale;                    /*!< 5x7 字体缩放倍数，推荐 2~4 */
    uint16_t bg_color;                /*!< 背景色，RGB565 */
    uint16_t label_color;             /*!< “FPS:” 颜色，RGB565 */
    uint16_t value_color;             /*!< 数字颜色，RGB565 */
    bsp_fps_overlay_align_t align;    /*!< 显示位置 */
    bool enabled;                     /*!< 是否显示 FPS */
} bsp_fps_overlay_config_t;

/**
 * @brief 初始化 FPS 叠加层。
 *
 * @param config 配置指针；传 NULL 时使用默认配置。
 * @return ESP_OK 或错误码。
 */
esp_err_t bsp_fps_overlay_init(const bsp_fps_overlay_config_t *config);

/**
 * @brief 释放 FPS 叠加层内部缓冲区。
 *
 * @return ESP_OK。
 */
esp_err_t bsp_fps_overlay_deinit(void);

/**
 * @brief 启用或关闭 FPS 显示。
 *
 * 关闭后仍然可以继续调用 bsp_fps_overlay_count_frame() 统计帧率，
 * 但 bsp_fps_overlay_draw() 不会向 LCD 绘制内容。
 */
void bsp_fps_overlay_set_enabled(bool enabled);

/**
 * @brief 查询 FPS 显示是否开启。
 */
bool bsp_fps_overlay_is_enabled(void);

/**
 * @brief 设置 FPS 绑定的画面区域。
 *
 * 该区域用于计算左上、右上、左下、右下等相对位置。
 */
esp_err_t bsp_fps_overlay_set_area(int x, int y, int w, int h);

/**
 * @brief 设置 FPS 显示位置。
 *
 * @param align 位置枚举。
 * @param margin_x 非自定义位置时的水平边距。
 * @param margin_y 非自定义位置时的垂直边距。
 * @return ESP_OK 或错误码。
 */
esp_err_t bsp_fps_overlay_set_position(bsp_fps_overlay_align_t align, int margin_x, int margin_y);

/**
 * @brief 设置 FPS 显示的自定义绝对坐标。
 */
esp_err_t bsp_fps_overlay_set_custom_position(int x, int y);

/**
 * @brief 通知 FPS 叠加层已经完成一帧显示。
 *
 * 建议在每次摄像头画面成功刷到 LCD 后调用一次。
 */
void bsp_fps_overlay_count_frame(void);

/**
 * @brief 获取当前 FPS，单位为 0.1 FPS。
 *
 * 例如返回 306 表示 30.6 FPS。
 */
uint32_t bsp_fps_overlay_get_fps_x10(void);

/**
 * @brief 绘制 FPS 叠加层。
 *
 * 如果显示关闭，函数直接返回 ESP_OK。
 */
esp_err_t bsp_fps_overlay_draw(void);

/**
 * @brief 将 FPS 叠加层绘制到一块 RGB565 画面缓冲区中。
 *
 * 该接口不会直接提交 LCD 刷新，适合摄像头预览这类高帧率场景：
 * 先把 FPS 文本合成到摄像头帧缓冲区，再把整帧一次性提交给 LCD。
 *
 * @param frame_buffer RGB565 画面缓冲区。
 * @param frame_w 缓冲区有效图像宽度，单位像素。
 * @param frame_h 缓冲区有效图像高度，单位像素。
 * @param stride_pixels 每一行占用的像素数；如果等于 0，则按 frame_w 处理。
 * @param screen_x 该缓冲区显示到屏幕时的左上角 X 坐标。
 * @param screen_y 该缓冲区显示到屏幕时的左上角 Y 坐标。
 * @return ESP_OK 或错误码。
 */
esp_err_t bsp_fps_overlay_draw_to_rgb565(uint16_t *frame_buffer,
                                         int frame_w,
                                         int frame_h,
                                         int stride_pixels,
                                         int screen_x,
                                         int screen_y);

#ifdef __cplusplus
}
#endif
