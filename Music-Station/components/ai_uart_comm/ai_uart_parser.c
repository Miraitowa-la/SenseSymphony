#include "ai_uart_parser.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void discard_pending(ai_uart_parser_t *parser)
{
    parser->have_header = false;
    parser->expected_count = 0;
    parser->received_count = 0;
    parser->pending.object_count = 0;
}

static bool parse_unsigned_field(const char **cursor,
                                 char delimiter,
                                 uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (cursor == NULL || *cursor == NULL || value == NULL ||
        **cursor < '0' || **cursor > '9') {
        return false;
    }

    errno = 0;
    parsed = strtoul(*cursor, &end, 10);
    if (errno == ERANGE || end == *cursor || parsed > UINT32_MAX ||
        *end != delimiter) {
        return false;
    }

    *value = (uint32_t)parsed;
    *cursor = delimiter == '\0' ? end : end + 1;
    return true;
}

static bool parse_signed_field(const char **cursor,
                               char delimiter,
                               long *value)
{
    char *end = NULL;
    long parsed;

    if (cursor == NULL || *cursor == NULL || value == NULL) {
        return false;
    }
    if (**cursor == '-') {
        if ((*cursor)[1] < '0' || (*cursor)[1] > '9') {
            return false;
        }
    } else if (**cursor < '0' || **cursor > '9') {
        return false;
    }

    errno = 0;
    parsed = strtol(*cursor, &end, 10);
    if (errno == ERANGE || end == *cursor || *end != delimiter) {
        return false;
    }

    *value = parsed;
    *cursor = delimiter == '\0' ? end : end + 1;
    return true;
}

static ai_uart_parse_result_t reject_line(ai_uart_parser_t *parser,
                                          bool rejected_header)
{
    if (rejected_header) {
        parser->stats.rejected_headers++;
    } else {
        parser->stats.malformed_lines++;
    }
    discard_pending(parser);
    return AI_UART_PARSE_REJECTED;
}

static ai_uart_parse_result_t parse_header(ai_uart_parser_t *parser,
                                           const char *line,
                                           uint32_t now_ms,
                                           ai_uart_snapshot_t *snapshot)
{
    const char *cursor = line + 2;
    uint32_t sequence;
    uint32_t count;
    long mode;

    if (!parse_unsigned_field(&cursor, ',', &sequence) ||
        !parse_signed_field(&cursor, ',', &mode) ||
        !parse_unsigned_field(&cursor, '\0', &count)) {
        return reject_line(parser, false);
    }
    if (mode < AI_UART_MODE_FACE || mode >= AI_UART_MODE_COUNT ||
        count > AI_UART_MAX_OBJECTS) {
        return reject_line(parser, true);
    }

    if (parser->have_header) {
        parser->stats.replaced_partials++;
    }
    memset(&parser->pending, 0, sizeof(parser->pending));
    parser->pending.sequence = sequence;
    parser->pending.mode = (ai_uart_mode_t)mode;
    parser->started_ms = now_ms;
    parser->expected_count = count;
    parser->received_count = 0;
    parser->have_header = true;

    if (count == 0) {
        *snapshot = parser->pending;
        parser->stats.valid_frames++;
        discard_pending(parser);
        return AI_UART_PARSE_SNAPSHOT;
    }
    return AI_UART_PARSE_NONE;
}

static ai_uart_parse_result_t parse_object(ai_uart_parser_t *parser,
                                           const char *line,
                                           ai_uart_snapshot_t *snapshot)
{
    const char *cursor = line + 2;
    uint32_t x10;
    uint32_t y10;
    uint32_t detect_confidence;
    uint32_t action_confidence;
    long action;

    if (!parser->have_header ||
        !parse_unsigned_field(&cursor, ',', &x10) ||
        !parse_unsigned_field(&cursor, ',', &y10) ||
        !parse_signed_field(&cursor, ',', &action) ||
        !parse_unsigned_field(&cursor, ',', &detect_confidence) ||
        !parse_unsigned_field(&cursor, '\0', &action_confidence) ||
        x10 > 1000 || y10 > 1000 || action < INT16_MIN ||
        action > INT16_MAX || detect_confidence > 1000 ||
        action_confidence > 1000 ||
        parser->received_count >= parser->expected_count) {
        return reject_line(parser, false);
    }

    ai_uart_object_t *object = &parser->pending.objects[parser->received_count];
    object->x10 = (uint16_t)x10;
    object->y10 = (uint16_t)y10;
    object->action = (int16_t)action;
    object->detect_confidence = (uint16_t)detect_confidence;
    object->action_confidence = (uint16_t)action_confidence;
    parser->received_count++;
    parser->pending.object_count = parser->received_count;

    if (parser->received_count == parser->expected_count) {
        *snapshot = parser->pending;
        parser->stats.valid_frames++;
        discard_pending(parser);
        return AI_UART_PARSE_SNAPSHOT;
    }
    return AI_UART_PARSE_NONE;
}

void ai_uart_parser_init(ai_uart_parser_t *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

ai_uart_parse_result_t ai_uart_parser_feed_line(
    ai_uart_parser_t *parser,
    const char *line,
    uint32_t now_ms,
    ai_uart_snapshot_t *snapshot)
{
    if (parser == NULL || line == NULL || snapshot == NULL) {
        return AI_UART_PARSE_REJECTED;
    }

    (void)ai_uart_parser_expire(parser, now_ms);
    if (line[0] == 'R' && line[1] == ',') {
        return parse_header(parser, line, now_ms, snapshot);
    }
    if (line[0] == 'O' && line[1] == ',') {
        return parse_object(parser, line, snapshot);
    }
    return reject_line(parser, false);
}

bool ai_uart_parser_expire(ai_uart_parser_t *parser, uint32_t now_ms)
{
    if (parser == NULL || !parser->have_header ||
        (uint32_t)(now_ms - parser->started_ms) <= AI_UART_FRAME_TIMEOUT_MS) {
        return false;
    }

    parser->stats.partial_timeouts++;
    discard_pending(parser);
    return true;
}

void ai_uart_parser_get_stats(const ai_uart_parser_t *parser,
                              ai_uart_parser_stats_t *stats)
{
    if (parser != NULL && stats != NULL) {
        *stats = parser->stats;
    }
}
