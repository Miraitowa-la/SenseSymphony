#include "audio_service.h"

#include <stdint.h>

#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t home_bgm_wav_start[] asm("_binary_home_bgm_wav_start");
extern const uint8_t home_bgm_wav_end[] asm("_binary_home_bgm_wav_end");

static TaskHandle_t s_bgm_task;
static volatile bool s_bgm_enabled;

static void home_bgm_task(void *arg)
{
    (void)arg;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (s_bgm_enabled) {
            const uint8_t *data = home_bgm_wav_start + 44;
            const uint8_t *end = home_bgm_wav_end;
            while (s_bgm_enabled && data < end) {
                size_t size = (size_t)(end - data);
                if (size > 4096) {
                    size = 4096;
                }
                if (bsp_audio_speaker_write(data, size) != ESP_OK) {
                    s_bgm_enabled = false;
                    break;
                }
                data += size;
            }
        }
    }
}

bool audio_service_set_home_bgm_enabled(bool enabled)
{
    if (!enabled) {
        s_bgm_enabled = false;
        return true;
    }

    if (bsp_audio_speaker_init(44100, 60) != ESP_OK) {
        return false;
    }
    if (s_bgm_task == NULL && xTaskCreate(home_bgm_task, "home_bgm", 4096, NULL, 4, &s_bgm_task) != pdPASS) {
        s_bgm_task = NULL;
        return false;
    }

    s_bgm_enabled = true;
    xTaskNotifyGive(s_bgm_task);
    return true;
}

bool audio_service_home_bgm_enabled(void)
{
    return s_bgm_enabled;
}
