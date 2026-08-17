#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MODE1_HISTORY_MAX_RECORDS 16
#define MODE1_HISTORY_MAX_EVENTS 32
typedef struct { uint8_t band; uint8_t note; uint16_t x10; uint16_t y10; uint32_t time_ms; } mode1_history_event_t;
bool mode1_history_init(void);
bool mode1_history_save(const mode1_history_event_t *events, uint8_t count);
bool mode1_history_delete(uint8_t slot);
uint8_t mode1_history_count(void);
bool mode1_history_get(uint8_t slot, mode1_history_event_t *events, uint8_t *count, uint32_t *sequence);
