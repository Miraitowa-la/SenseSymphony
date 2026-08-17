#pragma once

#include <stdbool.h>
#include <stdint.h>

void home_screen_create(void);
uint8_t home_screen_mode2_song_count(void);
uint8_t home_screen_mode2_song_id_limit(void);
bool home_screen_mode2_song_get(uint8_t id, const char **name);
uint8_t home_screen_mode2_selected_song(void);
bool home_screen_mode2_select_song(uint8_t id);
bool home_screen_mode2_delete_song(uint8_t id);
