#pragma once

#include <stddef.h>

#include "esp_err.h"

esp_err_t bsp_audio_speaker_init(unsigned sample_rate, unsigned volume);
esp_err_t bsp_audio_speaker_write(const void *data, size_t size);
