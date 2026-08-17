#include "face_detect_middleware.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "dl_image_define.hpp"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_log.h"
#include "human_face_detect.hpp"

static const char *TAG = "face_detect";
static std::unique_ptr<HumanFaceDetect> s_detector;

esp_err_t face_detect_middleware_init(void)
{
    if (s_detector) {
        return ESP_OK;
    }

    s_detector = std::make_unique<HumanFaceDetect>();
    ESP_RETURN_ON_FALSE(s_detector, ESP_ERR_NO_MEM, TAG, "create detector failed");
    ESP_LOGI(TAG, "human face detector ready");
    return ESP_OK;
}

bool face_detect_middleware_is_ready(void)
{
    return s_detector != nullptr;
}

const char *face_detect_middleware_expression_name(face_expression_t expression)
{
    switch (expression) {
    case FACE_EXPRESSION_NEUTRAL:
        return "neutral";
    case FACE_EXPRESSION_SMILE:
        return "smile";
    case FACE_EXPRESSION_UNKNOWN:
    default:
        return "unknown";
    }
}

static float abs_diff(int a, int b)
{
    return a > b ? static_cast<float>(a - b) : static_cast<float>(b - a);
}

static face_expression_t estimate_expression(const std::vector<int> &keypoint)
{
    if (keypoint.size() < 10) {
        return FACE_EXPRESSION_UNKNOWN;
    }

    const float eye_w = abs_diff(keypoint[0], keypoint[2]);
    const float mouth_w = abs_diff(keypoint[6], keypoint[8]);
    const float eye_y = (keypoint[1] + keypoint[3]) * 0.5f;
    const float mouth_y = (keypoint[7] + keypoint[9]) * 0.5f;
    const float face_mid_h = mouth_y - eye_y;

    if (eye_w <= 1.0f || face_mid_h <= 1.0f) {
        return FACE_EXPRESSION_UNKNOWN;
    }

    // ponytail: heuristic only; replace with a real expression model for anger/sad/surprise classes.
    return (mouth_w / eye_w > 0.78f) ? FACE_EXPRESSION_SMILE : FACE_EXPRESSION_NEUTRAL;
}

esp_err_t face_detect_middleware_detect_rgb565(const uint16_t *frame,
                                               int width,
                                               int height,
                                               face_detect_result_t *result)
{
    ESP_RETURN_ON_FALSE(frame, ESP_ERR_INVALID_ARG, TAG, "frame is NULL");
    ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "invalid frame size");
    ESP_RETURN_ON_FALSE(width <= UINT16_MAX && height <= UINT16_MAX, ESP_ERR_INVALID_ARG, TAG,
                        "frame size is too large");
    ESP_RETURN_ON_FALSE(result && result->boxes, ESP_ERR_INVALID_ARG, TAG, "result is invalid");

    if (!s_detector) {
        ESP_RETURN_ON_ERROR(face_detect_middleware_init(), TAG, "init detector failed");
    }

    result->count = 0;

    dl::image::img_t image = {
        .data = const_cast<uint16_t *>(frame),
        .width = static_cast<uint16_t>(width),
        .height = static_cast<uint16_t>(height),
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE,
    };

    auto detections = s_detector->run(image);
    for (const auto &detection : detections) {
        if (result->count >= result->capacity) {
            break;
        }

        face_detect_box_t *box = &result->boxes[result->count++];
        box->x1 = detection.box[0];
        box->y1 = detection.box[1];
        box->x2 = detection.box[2];
        box->y2 = detection.box[3];
        box->score = detection.score;
        box->expression = estimate_expression(detection.keypoint);
    }

    return ESP_OK;
}

static void draw_pixel(uint16_t *frame, int width, int height, int stride, int x, int y, uint16_t color)
{
    if (x >= 0 && x < width && y >= 0 && y < height) {
        frame[y * stride + x] = color;
    }
}

esp_err_t face_detect_middleware_draw_boxes_rgb565(uint16_t *frame,
                                                   int width,
                                                   int height,
                                                   int stride_pixels,
                                                   const face_detect_result_t *result)
{
    if (!frame || !result || stride_pixels < width) {
        return ESP_ERR_INVALID_ARG;
    }

    const int thickness = 3;

    for (size_t i = 0; i < result->count; i++) {
        const face_detect_box_t *box = &result->boxes[i];
        const uint16_t color = box->expression == FACE_EXPRESSION_SMILE ? 0x07E0 :
                               box->expression == FACE_EXPRESSION_NEUTRAL ? 0xF800 : 0xFFE0;
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
