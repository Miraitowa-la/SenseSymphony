#include "mode2_history_service.h"

#include <stdio.h>
#include <string.h>

#include "esp_spiffs.h"

#define MODE2_HISTORY_MAGIC 0x4d324852UL
#define MODE2_HISTORY_PATH "/storage/mode2_history.bin"

typedef struct {
    uint32_t magic;
    uint32_t next_sequence;
    uint8_t next_slot;
    uint8_t used[MODE2_HISTORY_MAX_RECORDS];
    uint32_t sequences[MODE2_HISTORY_MAX_RECORDS];
    mode2_history_result_t results[MODE2_HISTORY_MAX_RECORDS];
} mode2_history_file_t;

static mode2_history_file_t s_history;
static bool s_ready;

bool mode2_history_init(void)
{
    if (s_ready) return true;
    if (!esp_spiffs_mounted("storage")) {
        esp_vfs_spiffs_conf_t config = {
            .base_path = "/storage",
            .partition_label = "storage",
            .max_files = 2,
            .format_if_mount_failed = true,
        };
        if (esp_vfs_spiffs_register(&config) != ESP_OK) return false;
    }

    FILE *file = fopen(MODE2_HISTORY_PATH, "rb");
    if (!file || fread(&s_history, sizeof(s_history), 1, file) != 1 ||
        s_history.magic != MODE2_HISTORY_MAGIC) {
        memset(&s_history, 0, sizeof(s_history));
        s_history.magic = MODE2_HISTORY_MAGIC;
    }
    if (file) fclose(file);
    s_ready = true;
    return true;
}

static bool mode2_history_write(void)
{
    FILE *file = fopen(MODE2_HISTORY_PATH, "wb");
    if (!file) return false;
    bool ok = fwrite(&s_history, sizeof(s_history), 1, file) == 1;
    fclose(file);
    return ok;
}

bool mode2_history_save(const mode2_history_result_t *result)
{
    if (!s_ready || !result) return false;
    uint8_t slot = s_history.next_slot;
    s_history.results[slot] = *result;
    s_history.used[slot] = 1;
    s_history.sequences[slot] = ++s_history.next_sequence;
    s_history.next_slot = (slot + 1) % MODE2_HISTORY_MAX_RECORDS;
    return mode2_history_write();
}

uint8_t mode2_history_count(void)
{
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < MODE2_HISTORY_MAX_RECORDS; slot++) count += s_history.used[slot];
    return count;
}

bool mode2_history_get(uint8_t slot, mode2_history_result_t *result, uint32_t *sequence)
{
    if (!s_ready || slot >= MODE2_HISTORY_MAX_RECORDS || !s_history.used[slot]) return false;
    if (result) *result = s_history.results[slot];
    if (sequence) *sequence = s_history.sequences[slot];
    return true;
}

bool mode2_history_delete(uint8_t slot)
{
    if (!s_ready || slot >= MODE2_HISTORY_MAX_RECORDS || !s_history.used[slot]) return false;
    memset(&s_history.results[slot], 0, sizeof(s_history.results[slot]));
    s_history.used[slot] = 0;
    s_history.sequences[slot] = 0;
    return mode2_history_write();
}
