#include "mode1_history_service.h"
#include <stdio.h>
#include <string.h>
#include "esp_spiffs.h"
#define MODE1_HISTORY_MAGIC 0x4d314852UL
#define MODE1_HISTORY_PATH "/storage/mode1_history.bin"
typedef struct { uint32_t magic, next_sequence; uint8_t next_slot, counts[MODE1_HISTORY_MAX_RECORDS]; uint32_t sequences[MODE1_HISTORY_MAX_RECORDS]; mode1_history_event_t events[MODE1_HISTORY_MAX_RECORDS][MODE1_HISTORY_MAX_EVENTS]; } mode1_history_file_t;
static mode1_history_file_t s_history;
static bool s_ready;
bool mode1_history_init(void) {
    if (s_ready) return true;
    esp_vfs_spiffs_conf_t config = {.base_path="/storage", .partition_label="storage", .max_files=1, .format_if_mount_failed=true};
    if (esp_vfs_spiffs_register(&config) != ESP_OK) return false;
    FILE *file=fopen(MODE1_HISTORY_PATH,"rb");
    if (!file || fread(&s_history,sizeof(s_history),1,file)!=1 || s_history.magic!=MODE1_HISTORY_MAGIC) { memset(&s_history,0,sizeof(s_history)); s_history.magic=MODE1_HISTORY_MAGIC; }
    if (file) {
        fclose(file);
    }
    s_ready = true;
    return true;
}
static bool mode1_history_write(void) {
    FILE *file = fopen(MODE1_HISTORY_PATH, "wb");
    if (!file) return false;
    bool ok = fwrite(&s_history, sizeof(s_history), 1, file) == 1;
    fclose(file);
    return ok;
}
bool mode1_history_save(const mode1_history_event_t *events, uint8_t count) {
    if (!s_ready || !events || !count) return false;
    if (count > MODE1_HISTORY_MAX_EVENTS) count=MODE1_HISTORY_MAX_EVENTS;
    uint8_t slot=s_history.next_slot; memcpy(s_history.events[slot],events,count*sizeof(*events)); s_history.counts[slot]=count; s_history.sequences[slot]=++s_history.next_sequence; s_history.next_slot=(slot+1)%MODE1_HISTORY_MAX_RECORDS;
    return mode1_history_write();
}
bool mode1_history_delete(uint8_t slot) {
    if (!s_ready || slot >= MODE1_HISTORY_MAX_RECORDS || !s_history.counts[slot]) return false;
    memset(s_history.events[slot], 0, sizeof(s_history.events[slot]));
    s_history.counts[slot] = 0;
    s_history.sequences[slot] = 0;
    return mode1_history_write();
}
uint8_t mode1_history_count(void) { uint8_t count=0; for(uint8_t i=0;i<MODE1_HISTORY_MAX_RECORDS;i++) count+=s_history.counts[i]!=0; return count; }
bool mode1_history_get(uint8_t slot, mode1_history_event_t *events, uint8_t *count, uint32_t *sequence) {
    if(!s_ready || slot>=MODE1_HISTORY_MAX_RECORDS || !s_history.counts[slot]) return false;
    if (events) {
        memcpy(events, s_history.events[slot], s_history.counts[slot] * sizeof(*events));
    }
    if (count) {
        *count = s_history.counts[slot];
    }
    if (sequence) {
        *sequence = s_history.sequences[slot];
    }
    return true;
}
