#include "mode2_user_song_service.h"

#include <stdio.h>
#include <string.h>

#include "esp_spiffs.h"

#define MODE2_USER_SONG_MAGIC 0x4d325553UL
#define MODE2_USER_SONG_PATH "/storage/mode2_user_songs.bin"

typedef struct {
    uint32_t magic;
    uint8_t used[MODE2_USER_SONG_MAX];
    mode2_user_song_t songs[MODE2_USER_SONG_MAX];
} mode2_user_song_file_t;

static mode2_user_song_file_t s_file;
static bool s_ready;

bool mode2_user_song_init(void)
{
    if (s_ready) return true;
    if (!esp_spiffs_mounted("storage")) {
        esp_vfs_spiffs_conf_t config = {
            .base_path = "/storage",
            .partition_label = "storage",
            .max_files = 3,
            .format_if_mount_failed = true,
        };
        if (esp_vfs_spiffs_register(&config) != ESP_OK) return false;
    }
    FILE *file = fopen(MODE2_USER_SONG_PATH, "rb");
    if (!file || fread(&s_file, sizeof(s_file), 1, file) != 1 ||
        s_file.magic != MODE2_USER_SONG_MAGIC) {
        memset(&s_file, 0, sizeof(s_file));
        s_file.magic = MODE2_USER_SONG_MAGIC;
    }
    if (file) fclose(file);
    s_ready = true;
    return true;
}

static bool mode2_user_song_write(void)
{
    FILE *file = fopen(MODE2_USER_SONG_PATH, "wb");
    if (!file) return false;
    bool ok = fwrite(&s_file, sizeof(s_file), 1, file) == 1;
    fclose(file);
    return ok;
}

uint8_t mode2_user_song_count(void)
{
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < MODE2_USER_SONG_MAX; slot++) count += s_file.used[slot];
    return count;
}

const mode2_user_song_t *mode2_user_song_get(uint8_t slot)
{
    if (!s_ready || slot >= MODE2_USER_SONG_MAX || !s_file.used[slot]) return NULL;
    return &s_file.songs[slot];
}

bool mode2_user_song_save(const mode2_user_song_t *song, uint8_t *id)
{
    if (!s_ready || !song || !song->name[0] || !song->event_count ||
        song->event_count > MODE2_USER_SONG_MAX_EVENTS) return false;
    for (uint8_t slot = 0; slot < MODE2_USER_SONG_MAX; slot++) {
        if (s_file.used[slot]) continue;
        s_file.songs[slot] = *song;
        s_file.used[slot] = 1;
        if (!mode2_user_song_write()) {
            s_file.used[slot] = 0;
            return false;
        }
        if (id) *id = MODE2_USER_SONG_ID_BASE + slot;
        return true;
    }
    return false;
}

bool mode2_user_song_delete(uint8_t id)
{
    if (!s_ready || id < MODE2_USER_SONG_ID_BASE) return false;
    uint8_t slot = id - MODE2_USER_SONG_ID_BASE;
    if (slot >= MODE2_USER_SONG_MAX || !s_file.used[slot]) return false;
    memset(&s_file.songs[slot], 0, sizeof(s_file.songs[slot]));
    s_file.used[slot] = 0;
    return mode2_user_song_write();
}
