#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MODE2_USER_SONG_ID_BASE 16
#define MODE2_USER_SONG_MAX 8
#define MODE2_USER_SONG_MAX_EVENTS 42
#define MODE2_USER_SONG_NAME_LEN 21

typedef struct {
    uint32_t time_ms;
    uint8_t note;
} mode2_user_song_event_t;

typedef struct {
    char name[MODE2_USER_SONG_NAME_LEN];
    uint16_t beat_ms;
    uint8_t event_count;
    mode2_user_song_event_t events[MODE2_USER_SONG_MAX_EVENTS];
} mode2_user_song_t;

bool mode2_user_song_init(void);
uint8_t mode2_user_song_count(void);
const mode2_user_song_t *mode2_user_song_get(uint8_t slot);
bool mode2_user_song_save(const mode2_user_song_t *song, uint8_t *id);
bool mode2_user_song_delete(uint8_t id);
