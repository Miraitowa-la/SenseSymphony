#include "note_audio_service.h"

#include "audio_service.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct { const uint8_t *start; const uint8_t *end; } note_wav_t;
#define DECLARE_NOTE(band, number, note) \
extern const uint8_t face_##band##_##number##_##note##_start[] asm("_binary_face_" #band "_" #number "_" #note "_wav_start"); \
extern const uint8_t face_##band##_##number##_##note##_end[] asm("_binary_face_" #band "_" #number "_" #note "_wav_end")
#define NOTE(band, number, note) {face_##band##_##number##_##note##_start, face_##band##_##number##_##note##_end}
DECLARE_NOTE(low,1,do); DECLARE_NOTE(low,2,re); DECLARE_NOTE(low,3,mi); DECLARE_NOTE(low,4,fa); DECLARE_NOTE(low,5,sol); DECLARE_NOTE(low,6,la); DECLARE_NOTE(low,7,si);
DECLARE_NOTE(mid,1,do); DECLARE_NOTE(mid,2,re); DECLARE_NOTE(mid,3,mi); DECLARE_NOTE(mid,4,fa); DECLARE_NOTE(mid,5,sol); DECLARE_NOTE(mid,6,la); DECLARE_NOTE(mid,7,si);
DECLARE_NOTE(high,1,do); DECLARE_NOTE(high,2,re); DECLARE_NOTE(high,3,mi); DECLARE_NOTE(high,4,fa); DECLARE_NOTE(high,5,sol); DECLARE_NOTE(high,6,la); DECLARE_NOTE(high,7,si);
static const note_wav_t s_notes[3][7] = {
    {NOTE(low,1,do),NOTE(low,2,re),NOTE(low,3,mi),NOTE(low,4,fa),NOTE(low,5,sol),NOTE(low,6,la),NOTE(low,7,si)},
    {NOTE(mid,1,do),NOTE(mid,2,re),NOTE(mid,3,mi),NOTE(mid,4,fa),NOTE(mid,5,sol),NOTE(mid,6,la),NOTE(mid,7,si)},
    {NOTE(high,1,do),NOTE(high,2,re),NOTE(high,3,mi),NOTE(high,4,fa),NOTE(high,5,sol),NOTE(high,6,la),NOTE(high,7,si)},
};
static TaskHandle_t s_task;
static const note_wav_t *s_note;
static volatile uint32_t s_generation;
static bool s_enabled = true;
static uint8_t s_volume = 60;

static void note_audio_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const note_wav_t *note = s_note;
        uint32_t generation = s_generation;
        const uint8_t *data = note->start + 44;
        size_t frames = (size_t)(note->end - data) / 2U;
        uint32_t phase_q16 = 0;
        const uint32_t step_q16 = (16000U << 16) / 44100U;
        while ((phase_q16 >> 16) < frames && generation == s_generation) {
            int16_t stereo[128 * 2];
            size_t count = 0;
            while (count < 128U && (phase_q16 >> 16) < frames) {
                size_t index = phase_q16 >> 16;
                int16_t sample = (int16_t)((uint16_t)data[index * 2U] |
                                           ((uint16_t)data[index * 2U + 1U] << 8));
                stereo[count * 2U] = sample;
                stereo[count * 2U + 1U] = sample;
                ++count;
                phase_q16 += step_q16;
            }
            if (bsp_audio_speaker_write(stereo, count * sizeof(stereo[0])) != ESP_OK) break;
        }
    }
}

void note_audio_play(uint8_t band, uint8_t note)
{
    if (!s_enabled || band >= 3 || note >= 7 || bsp_audio_speaker_init(44100, s_volume) != ESP_OK) return;
    (void)audio_service_set_home_bgm_enabled(false);
    s_note = &s_notes[band][note];
    s_generation++;
    if (s_task == NULL && xTaskCreate(note_audio_task, "note_audio", 4096, NULL, 4, &s_task) != pdPASS) {
        s_task = NULL;
        return;
    }
    xTaskNotifyGive(s_task);
}

void note_audio_set_enabled(bool enabled) { s_enabled = enabled; }
bool note_audio_enabled(void) { return s_enabled; }
void note_audio_set_volume(uint8_t volume) { s_volume = volume > 100 ? 100 : volume; }
uint8_t note_audio_volume(void) { return s_volume; }
