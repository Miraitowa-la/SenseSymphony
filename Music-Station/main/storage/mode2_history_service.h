#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MODE2_HISTORY_MAX_RECORDS 16

typedef struct {
    uint8_t song_id;
    uint32_t score;
    uint16_t max_combo;
    uint16_t perfect;
    uint16_t great;
    uint16_t good;
    uint16_t miss;
    uint8_t accuracy;
} mode2_history_result_t;

bool mode2_history_init(void);
bool mode2_history_save(const mode2_history_result_t *result);
uint8_t mode2_history_count(void);
bool mode2_history_get(uint8_t slot, mode2_history_result_t *result, uint32_t *sequence);
bool mode2_history_delete(uint8_t slot);
