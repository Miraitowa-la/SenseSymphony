#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AI_UART_MAX_OBJECTS 4

typedef enum {
    AI_UART_MODE_FACE = 0,
    AI_UART_MODE_EXPR,
    AI_UART_MODE_HAND,
    AI_UART_MODE_GEST,
    AI_UART_MODE_COUNT,
} ai_uart_mode_t;

typedef struct {
    uint16_t x10;
    uint16_t y10;
    int16_t action;
    uint16_t detect_confidence;
    uint16_t action_confidence;
} ai_uart_object_t;

typedef struct {
    uint32_t sequence;
    ai_uart_mode_t mode;
    size_t object_count;
    ai_uart_object_t objects[AI_UART_MAX_OBJECTS];
} ai_uart_snapshot_t;

typedef struct {
    uint32_t valid_frames;
    uint32_t malformed_lines;
    uint32_t rejected_headers;
    uint32_t partial_timeouts;
    uint32_t replaced_partials;
} ai_uart_stats_t;

#ifdef __cplusplus
}
#endif
