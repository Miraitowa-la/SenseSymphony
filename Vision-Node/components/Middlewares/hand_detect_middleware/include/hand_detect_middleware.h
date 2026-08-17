#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
    float score;
    const char *gesture;
    float gesture_score;
} hand_detect_box_t;

typedef struct {
    hand_detect_box_t *boxes;
    size_t capacity;
    size_t count;
} hand_detect_result_t;

esp_err_t hand_detect_middleware_init(void);
bool hand_detect_middleware_is_ready(void);
esp_err_t hand_detect_middleware_detect_rgb565(const uint16_t *frame,
                                               int width,
                                               int height,
                                               hand_detect_result_t *result);
esp_err_t hand_detect_middleware_detect_rgb565_ex(const uint16_t *frame,
                                                  int width,
                                                  int height,
                                                  bool recognize_gesture,
                                                  hand_detect_result_t *result);
esp_err_t hand_detect_middleware_draw_boxes_rgb565(uint16_t *frame,
                                                   int width,
                                                   int height,
                                                   int stride_pixels,
                                                   const hand_detect_result_t *result);

#ifdef __cplusplus
}
#endif
