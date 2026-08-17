#pragma once

#include <stdbool.h>
#include <stdint.h>

void note_audio_play(uint8_t band, uint8_t note);
void note_audio_set_enabled(bool enabled);
bool note_audio_enabled(void);
void note_audio_set_volume(uint8_t volume);
uint8_t note_audio_volume(void);
