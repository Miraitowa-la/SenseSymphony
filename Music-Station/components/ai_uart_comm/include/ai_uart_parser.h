#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ai_uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AI_UART_FRAME_TIMEOUT_MS 200U

typedef enum {
    AI_UART_PARSE_NONE = 0,
    AI_UART_PARSE_SNAPSHOT,
    AI_UART_PARSE_REJECTED,
} ai_uart_parse_result_t;

typedef ai_uart_stats_t ai_uart_parser_stats_t;

typedef struct {
    ai_uart_snapshot_t pending;
    uint32_t started_ms;
    size_t expected_count;
    size_t received_count;
    bool have_header;
    ai_uart_parser_stats_t stats;
} ai_uart_parser_t;

void ai_uart_parser_init(ai_uart_parser_t *parser);
ai_uart_parse_result_t ai_uart_parser_feed_line(
    ai_uart_parser_t *parser,
    const char *line,
    uint32_t now_ms,
    ai_uart_snapshot_t *snapshot);
bool ai_uart_parser_expire(ai_uart_parser_t *parser, uint32_t now_ms);
void ai_uart_parser_get_stats(const ai_uart_parser_t *parser,
                              ai_uart_parser_stats_t *stats);

#ifdef __cplusplus
}
#endif
