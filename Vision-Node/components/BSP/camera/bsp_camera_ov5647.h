#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsp_camera_ov5647.h
 * @brief OV5647 MIPI-CSI 摄像头 BSP 接口。
 *
 * 本模块使用乐鑫官方 esp_cam_sensor、esp_driver_cam 和 esp_driver_isp 组件，
 * 将 OV5647 的 800x640 RAW8 MIPI-CSI 数据转换为 RGB565 帧缓冲区。
 */

#define BSP_CAMERA_OV5647_H_RES                 800
#define BSP_CAMERA_OV5647_V_RES                 640
#define BSP_CAMERA_OV5647_BITS_PER_PIXEL        16
#define BSP_CAMERA_OV5647_BYTES_PER_PIXEL       (BSP_CAMERA_OV5647_BITS_PER_PIXEL / 8)
#define BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT    3

#define BSP_CAMERA_OV5647_SCCB_I2C_PORT         0
#define BSP_CAMERA_OV5647_SCCB_SDA_GPIO         7
#define BSP_CAMERA_OV5647_SCCB_SCL_GPIO         8
#define BSP_CAMERA_OV5647_SCCB_CLK_HZ           100000

typedef struct {
    void *buffer;       /*!< RGB565 帧缓冲区指针 */
    size_t len;         /*!< 有效帧数据长度，单位字节 */
    uint32_t width;     /*!< 图像宽度 */
    uint32_t height;    /*!< 图像高度 */
} bsp_camera_frame_t;

/**
 * @brief 初始化 OV5647、MIPI-CSI 和 ISP。
 *
 * 初始化成功后，摄像头会持续采集 800x640 RAW8，并由 ISP 输出 RGB565。
 *
 * @return ESP_OK 或底层错误码。
 */
esp_err_t bsp_camera_ov5647_init(void);

/**
 * @brief 释放 OV5647 摄像头相关资源。
 *
 * @return ESP_OK 或底层错误码。
 */
esp_err_t bsp_camera_ov5647_deinit(void);

/**
 * @brief 等待并获取一帧 RGB565 图像。
 *
 * 调用者显示或处理完成后，必须调用 bsp_camera_ov5647_return_frame() 归还缓冲区。
 *
 * @param[out] frame 返回的帧信息。
 * @param timeout_ticks FreeRTOS tick 超时时间。
 * @return ESP_OK、ESP_ERR_TIMEOUT、ESP_ERR_INVALID_ARG 或 ESP_ERR_INVALID_STATE。
 */
esp_err_t bsp_camera_ov5647_get_frame(bsp_camera_frame_t *frame, TickType_t timeout_ticks);

/**
 * @brief 归还一帧图像缓冲区，让摄像头驱动继续复用。
 *
 * @param buffer 由 bsp_camera_ov5647_get_frame() 返回的 buffer。
 * @return ESP_OK 或错误码。
 */
esp_err_t bsp_camera_ov5647_return_frame(void *buffer);

#ifdef __cplusplus
}
#endif
