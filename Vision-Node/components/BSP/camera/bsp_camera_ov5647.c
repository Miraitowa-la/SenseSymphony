#include "bsp_camera_ov5647.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/isp.h"
#include "esp_attr.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sccb_i2c.h"
#include "esp_sccb_intf.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"

#define BSP_CAMERA_OV5647_FORMAT_NAME            "MIPI_2lane_24Minput_RAW8_800x640_50fps"
#define BSP_CAMERA_OV5647_FRAME_SIZE             \
    (BSP_CAMERA_OV5647_H_RES * BSP_CAMERA_OV5647_V_RES * BSP_CAMERA_OV5647_BYTES_PER_PIXEL)
#define BSP_CAMERA_OV5647_ISP_CLK_HZ             (80 * 1000 * 1000)
#define BSP_CAMERA_OV5647_FRAME_BUFFER_ALIGN     64

typedef struct {
    void *buffer;
    size_t len;
} camera_frame_item_t;

static const char *TAG = "bsp_camera";

static i2c_master_bus_handle_t s_i2c_bus;
static esp_sccb_io_handle_t s_sccb_io;
static esp_cam_sensor_device_t *s_sensor;
static esp_cam_ctlr_handle_t s_cam_ctlr;
static isp_proc_handle_t s_isp_proc;
static QueueHandle_t s_free_queue;
static QueueHandle_t s_ready_queue;
static void *s_frame_buffers[BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT];
static bool s_camera_initialized;

static void release_sccb_bus(void)
{
    if (s_sccb_io) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_sccb_del_i2c_io(s_sccb_io));
        s_sccb_io = NULL;
    }

    if (s_i2c_bus) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_del_master_bus(s_i2c_bus));
        s_i2c_bus = NULL;
    }
}

static bool IRAM_ATTR camera_get_new_buffer(esp_cam_ctlr_handle_t handle,
                                            esp_cam_ctlr_trans_t *trans,
                                            void *user_data)
{
    (void)handle;
    (void)user_data;

    void *buffer = NULL;
    BaseType_t need_yield = pdFALSE;
    BaseType_t received = pdFALSE;

    if (s_free_queue) {
        if (xPortInIsrContext()) {
            received = xQueueReceiveFromISR(s_free_queue, &buffer, &need_yield);
        } else {
            received = xQueueReceive(s_free_queue, &buffer, 0);
        }
    }

    if (received == pdTRUE && buffer) {
        trans->buffer = buffer;
        trans->buflen = BSP_CAMERA_OV5647_FRAME_SIZE;
    }

    return need_yield == pdTRUE;
}

static bool is_owned_frame_buffer(const void *buffer)
{
    for (int i = 0; i < BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT; i++) {
        if (buffer == s_frame_buffers[i]) {
            return true;
        }
    }

    return false;
}

static bool IRAM_ATTR camera_trans_finished(esp_cam_ctlr_handle_t handle,
                                            esp_cam_ctlr_trans_t *trans,
                                            void *user_data)
{
    (void)handle;
    (void)user_data;

    BaseType_t need_yield = pdFALSE;

    if (s_ready_queue && trans && is_owned_frame_buffer(trans->buffer)) {
        camera_frame_item_t item = {
            .buffer = trans->buffer,
            .len = trans->received_size ? trans->received_size : BSP_CAMERA_OV5647_FRAME_SIZE,
        };
        BaseType_t sent = pdFALSE;
        if (xPortInIsrContext()) {
            sent = xQueueSendFromISR(s_ready_queue, &item, &need_yield);
            if (sent != pdTRUE && s_free_queue) {
                void *buffer = trans->buffer;
                xQueueSendFromISR(s_free_queue, &buffer, &need_yield);
            }
        } else {
            sent = xQueueSend(s_ready_queue, &item, 0);
            if (sent != pdTRUE && s_free_queue) {
                void *buffer = trans->buffer;
                xQueueSend(s_free_queue, &buffer, 0);
            }
        }
    }

    return need_yield == pdTRUE;
}

static color_raw_element_order_t map_bayer_order(esp_cam_sensor_bayer_pattern_t bayer)
{
    switch (bayer) {
    case ESP_CAM_SENSOR_BAYER_RGGB:
        return COLOR_RAW_ELEMENT_ORDER_RGGB;
    case ESP_CAM_SENSOR_BAYER_GRBG:
        return COLOR_RAW_ELEMENT_ORDER_GRBG;
    case ESP_CAM_SENSOR_BAYER_GBRG:
        return COLOR_RAW_ELEMENT_ORDER_GBRG;
    case ESP_CAM_SENSOR_BAYER_BGGR:
    default:
        return COLOR_RAW_ELEMENT_ORDER_BGGR;
    }
}

static esp_err_t sensor_detect_and_start(const esp_cam_sensor_format_t **out_format)
{
    esp_err_t ret = ESP_OK;

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_CAMERA_OV5647_SCCB_SDA_GPIO,
        .scl_io_num = BSP_CAMERA_OV5647_SCCB_SCL_GPIO,
        .i2c_port = BSP_CAMERA_OV5647_SCCB_I2C_PORT,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_conf, &s_i2c_bus), TAG,
                        "create OV5647 SCCB I2C bus failed");

    esp_cam_sensor_config_t sensor_config = {
        .reset_pin = GPIO_NUM_NC,
        .pwdn_pin = GPIO_NUM_NC,
        .xclk_pin = GPIO_NUM_NC,
        .sensor_port = ESP_CAM_SENSOR_MIPI_CSI,
    };

    for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start;
         p < &__esp_cam_sensor_detect_fn_array_end; ++p) {
        sccb_i2c_config_t i2c_config = {
            .scl_speed_hz = BSP_CAMERA_OV5647_SCCB_CLK_HZ,
            .device_address = p->sccb_addr,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        };

        ret = sccb_new_i2c_io(s_i2c_bus, &i2c_config, &sensor_config.sccb_handle);
        if (ret != ESP_OK) {
            continue;
        }

        sensor_config.sensor_port = p->port;
        s_sensor = p->detect(&sensor_config);
        if (s_sensor) {
            s_sccb_io = sensor_config.sccb_handle;
            if (p->port != ESP_CAM_SENSOR_MIPI_CSI) {
                ESP_LOGE(TAG, "detected sensor is not on MIPI-CSI port");
                return ESP_ERR_NOT_SUPPORTED;
            }
            break;
        }

        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_sccb_del_i2c_io(sensor_config.sccb_handle));
        sensor_config.sccb_handle = NULL;
    }

    ESP_RETURN_ON_FALSE(s_sensor, ESP_ERR_NOT_FOUND, TAG, "OV5647 sensor not detected");
    ESP_LOGI(TAG, "camera sensor detected: %s", esp_cam_sensor_get_name(s_sensor));

    esp_cam_sensor_format_array_t fmt_array = {0};
    ESP_RETURN_ON_ERROR(esp_cam_sensor_query_format(s_sensor, &fmt_array), TAG, "query camera formats failed");

    const esp_cam_sensor_format_t *selected_format = NULL;
    for (uint32_t i = 0; i < fmt_array.count; i++) {
        ESP_LOGI(TAG, "camera format[%lu]: %s", (unsigned long)i, fmt_array.format_array[i].name);
        if (strcmp(fmt_array.format_array[i].name, BSP_CAMERA_OV5647_FORMAT_NAME) == 0) {
            selected_format = &fmt_array.format_array[i];
        }
    }
    ESP_RETURN_ON_FALSE(selected_format, ESP_ERR_NOT_SUPPORTED, TAG,
                        "camera format %s is not enabled", BSP_CAMERA_OV5647_FORMAT_NAME);

    ESP_RETURN_ON_ERROR(esp_cam_sensor_set_format(s_sensor, selected_format), TAG,
                        "set OV5647 format failed");

    int stream_on = 1;
    ESP_RETURN_ON_ERROR(esp_cam_sensor_ioctl(s_sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &stream_on), TAG,
                        "start OV5647 stream failed");

    *out_format = selected_format;
    ESP_LOGI(TAG, "OV5647 stream started: %s", selected_format->name);
    release_sccb_bus();
    return ESP_OK;
}

static esp_err_t alloc_frame_buffers(void)
{
    s_free_queue = xQueueCreate(BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT, sizeof(void *));
    ESP_RETURN_ON_FALSE(s_free_queue, ESP_ERR_NO_MEM, TAG, "create free frame queue failed");

    s_ready_queue = xQueueCreate(BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT, sizeof(camera_frame_item_t));
    ESP_RETURN_ON_FALSE(s_ready_queue, ESP_ERR_NO_MEM, TAG, "create ready frame queue failed");

    for (int i = 0; i < BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT; i++) {
        s_frame_buffers[i] = heap_caps_aligned_alloc(BSP_CAMERA_OV5647_FRAME_BUFFER_ALIGN,
                                                     BSP_CAMERA_OV5647_FRAME_SIZE,
                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(s_frame_buffers[i], ESP_ERR_NO_MEM, TAG, "allocate camera frame buffer failed");
        ESP_RETURN_ON_FALSE(xQueueSend(s_free_queue, &s_frame_buffers[i], 0) == pdTRUE,
                            ESP_FAIL, TAG, "queue camera frame buffer failed");
    }

    return ESP_OK;
}

esp_err_t bsp_camera_ov5647_init(void)
{
    if (s_camera_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    const esp_cam_sensor_format_t *sensor_format = NULL;

    ESP_GOTO_ON_ERROR(alloc_frame_buffers(), err, TAG, "allocate camera buffers failed");
    ESP_GOTO_ON_ERROR(sensor_detect_and_start(&sensor_format), err, TAG, "init OV5647 sensor failed");

    uint32_t lane_bit_rate_mbps = sensor_format->mipi_info.mipi_clk /
                                  sensor_format->mipi_info.lane_num / 1000000;

    esp_cam_ctlr_csi_config_t csi_config = {
        .ctlr_id = 0,
        .h_res = BSP_CAMERA_OV5647_H_RES,
        .v_res = BSP_CAMERA_OV5647_V_RES,
        .lane_bit_rate_mbps = lane_bit_rate_mbps,
        .input_data_color_type = CAM_CTLR_COLOR_RAW8,
        .output_data_color_type = CAM_CTLR_COLOR_RGB565,
        .data_lane_num = sensor_format->mipi_info.lane_num,
        .queue_items = BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT,
        .byte_swap_en = false,
    };
    ESP_GOTO_ON_ERROR(esp_cam_new_csi_ctlr(&csi_config, &s_cam_ctlr), err, TAG,
                      "create CSI controller failed");

    esp_cam_ctlr_evt_cbs_t cbs = {
        .on_get_new_trans = camera_get_new_buffer,
        .on_trans_finished = camera_trans_finished,
    };
    ESP_GOTO_ON_ERROR(esp_cam_ctlr_register_event_callbacks(s_cam_ctlr, &cbs, NULL), err, TAG,
                      "register CSI callbacks failed");
    ESP_GOTO_ON_ERROR(esp_cam_ctlr_enable(s_cam_ctlr), err, TAG, "enable CSI controller failed");

    esp_isp_processor_cfg_t isp_config = {
        .clk_hz = BSP_CAMERA_OV5647_ISP_CLK_HZ,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
        .has_line_start_packet = sensor_format->mipi_info.line_sync_en,
        .has_line_end_packet = sensor_format->mipi_info.line_sync_en,
        .h_res = BSP_CAMERA_OV5647_H_RES,
        .v_res = BSP_CAMERA_OV5647_V_RES,
        .bayer_order = sensor_format->isp_info ?
                       map_bayer_order(sensor_format->isp_info->isp_v1_info.bayer_type) :
                       COLOR_RAW_ELEMENT_ORDER_GBRG,
    };
    ESP_GOTO_ON_ERROR(esp_isp_new_processor(&isp_config, &s_isp_proc), err, TAG,
                      "create ISP processor failed");
    ESP_GOTO_ON_ERROR(esp_isp_enable(s_isp_proc), err, TAG, "enable ISP processor failed");
    ESP_GOTO_ON_ERROR(esp_cam_ctlr_start(s_cam_ctlr), err, TAG, "start CSI controller failed");

    s_camera_initialized = true;
    ESP_LOGI(TAG, "OV5647 camera ready: %ux%u RGB565, lane bitrate %lu Mbps",
             (unsigned)BSP_CAMERA_OV5647_H_RES,
             (unsigned)BSP_CAMERA_OV5647_V_RES,
             (unsigned long)lane_bit_rate_mbps);
    return ESP_OK;

err:
    bsp_camera_ov5647_deinit();
    return ret;
}

esp_err_t bsp_camera_ov5647_deinit(void)
{
    esp_err_t ret = ESP_OK;

    if (s_cam_ctlr) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_cam_ctlr_stop(s_cam_ctlr));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_cam_ctlr_disable(s_cam_ctlr));
        ret = esp_cam_ctlr_del(s_cam_ctlr);
        s_cam_ctlr = NULL;
    }

    if (s_isp_proc) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_isp_disable(s_isp_proc));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_isp_del_processor(s_isp_proc));
        s_isp_proc = NULL;
    }

    if (s_sensor) {
        int stream_off = 0;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_cam_sensor_ioctl(s_sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &stream_off));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_cam_sensor_del_dev(s_sensor));
        s_sensor = NULL;
    }

    release_sccb_bus();

    for (int i = 0; i < BSP_CAMERA_OV5647_FRAME_BUFFER_COUNT; i++) {
        if (s_frame_buffers[i]) {
            heap_caps_free(s_frame_buffers[i]);
            s_frame_buffers[i] = NULL;
        }
    }

    if (s_free_queue) {
        vQueueDelete(s_free_queue);
        s_free_queue = NULL;
    }

    if (s_ready_queue) {
        vQueueDelete(s_ready_queue);
        s_ready_queue = NULL;
    }

    s_camera_initialized = false;
    return ret;
}

esp_err_t bsp_camera_ov5647_get_frame(bsp_camera_frame_t *frame, TickType_t timeout_ticks)
{
    ESP_RETURN_ON_FALSE(frame, ESP_ERR_INVALID_ARG, TAG, "frame is NULL");
    ESP_RETURN_ON_FALSE(s_camera_initialized && s_ready_queue, ESP_ERR_INVALID_STATE, TAG,
                        "camera is not initialized");

    camera_frame_item_t item = {0};
    if (xQueueReceive(s_ready_queue, &item, timeout_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    frame->buffer = item.buffer;
    frame->len = item.len;
    frame->width = BSP_CAMERA_OV5647_H_RES;
    frame->height = BSP_CAMERA_OV5647_V_RES;
    return ESP_OK;
}

esp_err_t bsp_camera_ov5647_return_frame(void *buffer)
{
    ESP_RETURN_ON_FALSE(buffer, ESP_ERR_INVALID_ARG, TAG, "buffer is NULL");
    ESP_RETURN_ON_FALSE(s_camera_initialized && s_free_queue, ESP_ERR_INVALID_STATE, TAG,
                        "camera is not initialized");
    ESP_RETURN_ON_FALSE(is_owned_frame_buffer(buffer), ESP_ERR_INVALID_ARG, TAG,
                        "buffer does not belong to camera BSP");

    return xQueueSend(s_free_queue, &buffer, pdMS_TO_TICKS(10)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
