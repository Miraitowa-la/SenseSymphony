#include "hand_detect_middleware.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "dl_image_define.hpp"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_log.h"
#include "hand_detect.hpp"
#include "hand_gesture_recognition.hpp"

static const char *TAG = "hand_detect";
static std::unique_ptr<HandDetect> s_detector;
static std::unique_ptr<HandGestureRecognizer> s_gesture_recognizer;

esp_err_t hand_detect_middleware_init(void)
{
    if (s_detector) {
        return ESP_OK;
    }

    s_detector = std::make_unique<HandDetect>();
    ESP_RETURN_ON_FALSE(s_detector, ESP_ERR_NO_MEM, TAG, "create detector failed");
    s_gesture_recognizer = std::make_unique<HandGestureRecognizer>();
    ESP_RETURN_ON_FALSE(s_gesture_recognizer, ESP_ERR_NO_MEM, TAG, "create gesture recognizer failed");
    ESP_LOGI(TAG, "hand detector and gesture recognizer ready");
    return ESP_OK;
}

bool hand_detect_middleware_is_ready(void)
{
    return s_detector != nullptr && s_gesture_recognizer != nullptr;
}

esp_err_t hand_detect_middleware_detect_rgb565(const uint16_t *frame,
                                               int width,
                                               int height,
                                               hand_detect_result_t *result)
{
    return hand_detect_middleware_detect_rgb565_ex(frame, width, height, true, result);
}

esp_err_t hand_detect_middleware_detect_rgb565_ex(const uint16_t *frame,
                                                  int width,
                                                  int height,
                                                  bool recognize_gesture,
                                                  hand_detect_result_t *result)
{
    ESP_RETURN_ON_FALSE(frame, ESP_ERR_INVALID_ARG, TAG, "frame is NULL");
    ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "invalid frame size");
    ESP_RETURN_ON_FALSE(width <= UINT16_MAX && height <= UINT16_MAX, ESP_ERR_INVALID_ARG, TAG,
                        "frame size is too large");
    ESP_RETURN_ON_FALSE(result && result->boxes, ESP_ERR_INVALID_ARG, TAG, "result is invalid");

    if (!s_detector) {
        ESP_RETURN_ON_ERROR(hand_detect_middleware_init(), TAG, "init detector failed");
    }

    result->count = 0;

    dl::image::img_t image = {
        .data = const_cast<uint16_t *>(frame),
        .width = static_cast<uint16_t>(width),
        .height = static_cast<uint16_t>(height),
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE,
    };

    auto detections = s_detector->run(image);
    std::vector<dl::cls::result_t> gestures;
    if (recognize_gesture && s_gesture_recognizer && !detections.empty()) {
        gestures = s_gesture_recognizer->recognize(image, detections);
    }

    size_t gesture_index = 0;
    for (const auto &detection : detections) {
        if (result->count >= result->capacity) {
            break;
        }

        hand_detect_box_t *box = &result->boxes[result->count++];
        box->x1 = detection.box[0];
        box->y1 = detection.box[1];
        box->x2 = detection.box[2];
        box->y2 = detection.box[3];
        box->score = detection.score;
        box->gesture = "unknown";
        box->gesture_score = 0.0f;
        if (gesture_index < gestures.size()) {
            box->gesture = gestures[gesture_index].cat_name ? gestures[gesture_index].cat_name : "unknown";
            box->gesture_score = gestures[gesture_index].score;
            gesture_index++;
        }
    }

    return ESP_OK;
}

static void draw_pixel(uint16_t *frame, int width, int height, int stride, int x, int y, uint16_t color)
{
    if (x >= 0 && x < width && y >= 0 && y < height) {
        frame[y * stride + x] = color;
    }
}

esp_err_t hand_detect_middleware_draw_boxes_rgb565(uint16_t *frame,
                                                   int width,
                                                   int height,
                                                   int stride_pixels,
                                                   const hand_detect_result_t *result)
{
    if (!frame || !result || stride_pixels < width) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint16_t color = 0x07E0;
    const int thickness = 3;

    for (size_t i = 0; i < result->count; i++) {
        const hand_detect_box_t *box = &result->boxes[i];
        int x1 = std::clamp(box->x1, 0, width - 1);
        int y1 = std::clamp(box->y1, 0, height - 1);
        int x2 = std::clamp(box->x2, 0, width - 1);
        int y2 = std::clamp(box->y2, 0, height - 1);

        for (int t = 0; t < thickness; t++) {
            for (int x = x1; x <= x2; x++) {
                draw_pixel(frame, width, height, stride_pixels, x, y1 + t, color);
                draw_pixel(frame, width, height, stride_pixels, x, y2 - t, color);
            }
            for (int y = y1; y <= y2; y++) {
                draw_pixel(frame, width, height, stride_pixels, x1 + t, y, color);
                draw_pixel(frame, width, height, stride_pixels, x2 - t, y, color);
            }
        }
    }

    return esp_cache_msync(frame,
                           stride_pixels * height * sizeof(uint16_t),
                           ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}
