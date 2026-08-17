#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FACE_EXPRESSION_UNKNOWN = 0,
    FACE_EXPRESSION_NEUTRAL,
    FACE_EXPRESSION_SMILE,
} face_expression_t;

typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
    float score;
    face_expression_t expression;
} face_detect_box_t;

typedef struct {
    face_detect_box_t *boxes;
    size_t capacity;
    size_t count;
} face_detect_result_t;

esp_err_t face_detect_middleware_init(void);
bool face_detect_middleware_is_ready(void);
const char *face_detect_middleware_expression_name(face_expression_t expression);
esp_err_t face_detect_middleware_detect_rgb565(const uint16_t *frame,
                                               int width,
                                               int height,
                                               face_detect_result_t *result);
esp_err_t face_detect_middleware_draw_boxes_rgb565(uint16_t *frame,
                                                   int width,
                                                   int height,
                                                   int stride_pixels,
                                                   const face_detect_result_t *result);

#ifdef __cplusplus
}
#endif
