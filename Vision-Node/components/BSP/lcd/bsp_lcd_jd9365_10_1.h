#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsp_lcd_jd9365_10_1.h
 * @brief Waveshare 10.1 寸 JD9365 MIPI-DSI LCD + GT9271 触摸 BSP 接口。
 *
 * 本文件面向应用层提供简洁的板级接口，屏蔽 esp_lcd、MIPI-DSI、JD9365
 * 面板驱动、GT9271 触摸寄存器读取等底层细节。应用层通常只需要调用：
 *
 * 1. bsp_lcd_init() 初始化屏幕；
 * 2. bsp_touch_init() 初始化触摸；
 * 3. bsp_lcd_draw_bitmap() 刷新像素区域；
 * 4. bsp_touch_read_points() 读取触摸点。
 */

/**
 * @name LCD 基础参数
 * @{
 *
 * 当前参数匹配 waveshare/esp_lcd_jd9365_10_1 组件示例：
 * 800x1280、RGB565、MIPI-DSI 2 lane。
 */
#define BSP_LCD_H_RES                    800
#define BSP_LCD_V_RES                    1280
#define BSP_LCD_BITS_PER_PIXEL           16
#define BSP_LCD_BYTES_PER_PIXEL          (BSP_LCD_BITS_PER_PIXEL / 8)
/** @} */

/**
 * @name LCD GPIO 与电源参数
 * @{
 *
 * 如果硬件连接与 Waveshare 示例不同，应优先修改这里。
 */
#define BSP_LCD_RST_GPIO                 27
#define BSP_LCD_BK_LIGHT_GPIO            26
#define BSP_LCD_BK_LIGHT_ON_LEVEL        1
#define BSP_LCD_BK_LIGHT_OFF_LEVEL       (!BSP_LCD_BK_LIGHT_ON_LEVEL)
#define BSP_LCD_MIPI_DSI_LANE_NUM        2
#define BSP_LCD_MIPI_PHY_LDO_CHAN        3
#define BSP_LCD_MIPI_PHY_LDO_VOLTAGE_MV  2500
/** @} */

/**
 * @name GT9271 十点触摸参数
 * @{
 *
 * 该屏幕的触摸芯片为 Goodix GT9271，通常支持最多 10 点触摸。
 * 本 BSP 默认复用 I2C1 / GPIO7 / GPIO8。注意 JD9365 面板组件内部也会使用
 * 同一组 I2C 引脚写入板载器件寄存器。
 */
#define BSP_TOUCH_MAX_POINTS             10
#define BSP_TOUCH_I2C_PORT               1
#define BSP_TOUCH_I2C_SDA_GPIO           7
#define BSP_TOUCH_I2C_SCL_GPIO           8
#define BSP_TOUCH_I2C_CLK_HZ             400000
#define BSP_TOUCH_GT9271_ADDR_PRIMARY    0x14
#define BSP_TOUCH_GT9271_ADDR_SECONDARY  0x5D
/** @} */

/**
 * @brief 单个触摸点数据。
 */
typedef struct {
    uint16_t x;      /*!< 触摸点 X 坐标，单位为屏幕像素 */
    uint16_t y;      /*!< 触摸点 Y 坐标，单位为屏幕像素 */
    uint16_t size;   /*!< 触摸面积或压力近似值，具体含义由 GT9271 固件决定 */
    uint8_t id;      /*!< 触摸点跟踪 ID，用于区分多点触摸中的不同手指 */
} bsp_touch_point_t;

/**
 * @brief 一次触摸采样结果。
 */
typedef struct {
    uint8_t count;                                      /*!< 当前有效触摸点数量，范围 0 到 BSP_TOUCH_MAX_POINTS */
    bsp_touch_point_t points[BSP_TOUCH_MAX_POINTS];     /*!< 触摸点数组，仅前 count 项有效 */
} bsp_touch_data_t;

/**
 * @brief 初始化 JD9365 10.1 寸 MIPI-DSI 屏幕。
 *
 * 初始化流程包括：
 * - 配置并打开背光 GPIO；
 * - 打开 MIPI DSI PHY LDO；
 * - 创建 MIPI DSI bus；
 * - 创建 MIPI DBI command IO；
 * - 创建并初始化 JD9365 panel；
 * - 打开显示；
 * - 注册 DPI 传输完成回调。
 *
 * @return
 * - ESP_OK：初始化成功，或者屏幕已经初始化；
 * - ESP_ERR_NO_MEM：信号量等资源分配失败；
 * - 其他 esp_err_t：GPIO、LDO、DSI、panel 初始化失败。
 */
esp_err_t bsp_lcd_init(void);

/**
 * @brief 释放 LCD 和触摸相关资源。
 *
 * 释放顺序为 panel、panel IO、DSI bus、MIPI PHY LDO、触摸设备、刷新信号量、
 * 背光 GPIO。该函数适合测试或低功耗场景使用，普通 UI 应用通常不需要频繁调用。
 *
 * @return ESP_OK 或最后一次释放动作返回的错误码。
 */
esp_err_t bsp_lcd_deinit(void);

/**
 * @brief 获取底层 esp_lcd panel handle。
 *
 * 当应用需要直接调用 esp_lcd 或 esp_lcd_dpi_panel_* API 时，可通过该函数取得
 * panel handle。普通绘图推荐优先使用 bsp_lcd_draw_bitmap()。
 *
 * @return 已初始化的 panel handle；若 LCD 尚未初始化，则返回 NULL。
 */
esp_lcd_panel_handle_t bsp_lcd_get_panel(void);

/**
 * @brief 打开或关闭背光。
 *
 * @param on true 打开背光，false 关闭背光。
 * @return ESP_OK 或 GPIO 操作错误码。
 */
esp_err_t bsp_lcd_set_backlight(bool on);

/**
 * @brief 打开或关闭 LCD 面板显示。
 *
 * 该函数发送 LCD Display ON/OFF 命令，不等同于背光控制。
 *
 * @param on true 打开显示，false 关闭显示。
 * @return ESP_OK、ESP_ERR_INVALID_STATE 或面板命令错误码。
 */
esp_err_t bsp_lcd_display_on(bool on);

/**
 * @brief 设置颜色反相。
 *
 * @param invert true 颜色反相，false 取消颜色反相。
 * @return ESP_OK、ESP_ERR_INVALID_STATE 或面板命令错误码。
 */
esp_err_t bsp_lcd_invert_color(bool invert);

/**
 * @brief 设置镜像显示。
 *
 * JD9365 组件支持 X/Y 镜像，但不支持 swap_xy，因此不能仅靠该接口完成
 * 90 度或 270 度旋转。
 *
 * @param mirror_x true 启用 X 方向镜像。
 * @param mirror_y true 启用 Y 方向镜像。
 * @return ESP_OK、ESP_ERR_INVALID_STATE 或面板命令错误码。
 */
esp_err_t bsp_lcd_mirror(bool mirror_x, bool mirror_y);

/**
 * @brief 向屏幕指定区域提交像素数据。
 *
 * 坐标区间遵循 esp_lcd 约定：左上角包含，右下角不包含，
 * 即 [x_start, x_end) x [y_start, y_end)。
 *
 * color_data 必须与当前屏幕像素格式匹配。本 BSP 默认使用 RGB565，
 * 因此每个像素需要 2 字节，通常可使用 uint16_t 数组保存像素。
 *
 * @param x_start 区域左上角 X 坐标。
 * @param y_start 区域左上角 Y 坐标。
 * @param x_end 区域右下角 X 坐标，不包含。
 * @param y_end 区域右下角 Y 坐标，不包含。
 * @param color_data 像素数据指针，建议位于 DMA 可访问内存。
 * @return ESP_OK、ESP_ERR_INVALID_ARG、ESP_ERR_INVALID_STATE 或底层绘图错误码。
 */
esp_err_t bsp_lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);

/**
 * @brief 等待 DPI 颜色数据传输完成。
 *
 * 如果应用复用同一块 DMA 绘图缓冲区，应在每次 bsp_lcd_draw_bitmap() 后等待
 * 传输完成，再改写该缓冲区。否则可能出现花屏或颜色数据被覆盖。
 *
 * @param timeout_ticks FreeRTOS tick 超时时间，可使用 portMAX_DELAY 永久等待。
 * @return ESP_OK、ESP_ERR_TIMEOUT 或 ESP_ERR_INVALID_STATE。
 */
esp_err_t bsp_lcd_wait_flush_done(TickType_t timeout_ticks);

/**
 * @brief 初始化 GT9271 十点触摸。
 *
 * 该函数创建 I2C bus 和 GT9271 设备句柄，并尝试读取 Product ID 验证设备。
 * 默认先尝试地址 0x14，失败后尝试 0x5D。
 *
 * @return
 * - ESP_OK：初始化成功，或者触摸已经初始化；
 * - ESP_ERR_NOT_FOUND：两个地址都未发现 GT9271；
 * - 其他 esp_err_t：I2C 读写失败。
 */
esp_err_t bsp_touch_init(void);

/**
 * @brief 释放触摸设备资源。
 *
 * 该函数会删除触摸设备句柄，但不会主动删除 I2C1 bus。原因是 LCD 驱动内部也会
 * 使用同一条 I2C bus 控制屏幕板载器件。
 *
 * @return ESP_OK。
 */
esp_err_t bsp_touch_deinit(void);

/**
 * @brief 读取当前触摸点。
 *
 * 如果 GT9271 状态寄存器未置位，函数返回 ESP_OK 且 data->count 为 0。
 * 如果存在触摸点，函数读取最多 BSP_TOUCH_MAX_POINTS 个点，并在读取后清除
 * GT9271 状态寄存器，准备下一次采样。
 *
 * @param[out] data 触摸采样结果，不可为 NULL。
 * @return ESP_OK、ESP_ERR_INVALID_ARG、ESP_ERR_INVALID_STATE 或 I2C 读写错误码。
 */
esp_err_t bsp_touch_read_points(bsp_touch_data_t *data);

#ifdef __cplusplus
}
#endif
