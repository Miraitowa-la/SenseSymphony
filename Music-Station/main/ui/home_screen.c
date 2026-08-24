#include "home_screen.h"

#include "audio/audio_service.h"
#include "audio/note_audio_service.h"
#include "storage/mode1_history_service.h"
#include "storage/mode2_history_service.h"
#include "storage/mode2_user_song_service.h"
#include "uart/ai_uart_service.h"
#include "ai_uart_comm.h"
#include "esp_log.h"
#include "lvgl.h"

LV_IMAGE_DECLARE(hom);
LV_IMAGE_DECLARE(free_mode_bg_2);
LV_IMAGE_DECLARE(img_note_a);
LV_IMAGE_DECLARE(img_note_b);
LV_IMAGE_DECLARE(img_note_c);
LV_IMAGE_DECLARE(img_note_d);
LV_IMAGE_DECLARE(img_note_e);
LV_IMAGE_DECLARE(img_note_f);
LV_IMAGE_DECLARE(img_note_g);
LV_IMAGE_DECLARE(practice_bg);
LV_IMAGE_DECLARE(judge_perfect);
LV_IMAGE_DECLARE(judge_great);
LV_IMAGE_DECLARE(judge_good);
LV_IMAGE_DECLARE(judge_miss);
LV_IMAGE_DECLARE(note_lane1_star);
LV_IMAGE_DECLARE(note_lane2_diamond);
LV_IMAGE_DECLARE(note_lane3_circle);
LV_IMAGE_DECLARE(note_lane4_star_purple);
LV_IMAGE_DECLARE(note_lane5_heart);
LV_IMAGE_DECLARE(note_lane6_hexagon);
LV_IMAGE_DECLARE(note_lane1_star_40);
LV_IMAGE_DECLARE(note_lane2_diamond_40);
LV_IMAGE_DECLARE(note_lane3_circle_40);
LV_IMAGE_DECLARE(note_lane4_star_purple_40);
LV_IMAGE_DECLARE(note_lane5_heart_40);
LV_IMAGE_DECLARE(note_lane6_hexagon_40);
LV_IMAGE_DECLARE(note_lane1_star_52);
LV_IMAGE_DECLARE(note_lane2_diamond_52);
LV_IMAGE_DECLARE(note_lane3_circle_52);
LV_IMAGE_DECLARE(note_lane4_star_purple_52);
LV_IMAGE_DECLARE(note_lane5_heart_52);
LV_IMAGE_DECLARE(note_lane6_hexagon_52);
LV_IMAGE_DECLARE(hit_ring_perfect);
LV_IMAGE_DECLARE(hit_ring_great);
LV_IMAGE_DECLARE(hit_ring_good);
LV_IMAGE_DECLARE(hit_ring_miss);
LV_IMAGE_DECLARE(countdown_3);
LV_IMAGE_DECLARE(countdown_2);
LV_IMAGE_DECLARE(countdown_1);
LV_IMAGE_DECLARE(countdown_go);

#define MODE1_UART_POLL_MS 40U
#define MODE1_CONF_LOW_THRESHOLD 500U
#define MODE1_CONF_HIGH_THRESHOLD 800U
#define MODE1_CONFIRM_FRAMES 2U
#define MODE1_NOTE_CHANGE_INTERVAL_MS 120U
#define MODE1_GESTURE_LOST_GRACE_MS 350U
#define MODE1_RECORD_MAX 32U
#define MODE1_TIMELINE_STEPS 12U
#define MODE2_UART_POLL_MS 40U
#define MODE2_NOTE_FALL_MS 4200U
#define MODE2_BEAT_MS 600U
#define MODE2_SONG_TICK_MS 33U
#define MODE2_SONG_MAX_NOTES 42U
#define MODE2_LANE_TOP_LEFT_X 300
#define MODE2_LANE_TOP_RIGHT_X 500
#define MODE2_LANE_TOP_Y 405
#define MODE2_LANE_JUDGE_LEFT_X 43
#define MODE2_LANE_JUDGE_RIGHT_X 755
#define MODE2_LANE_JUDGE_Y 810
#define MODE2_PERFECT_MS 50U
#define MODE2_GREAT_MS 100U
#define MODE2_GOOD_MS 150U
#define MODE2_COUNTDOWN_MS 800U
#define MODE3_UART_POLL_MS 40U
#define MODE3_DEBOUNCE_FRAMES 3U
#define MODE3_NOTE_CHANGE_MIN_GAP_MS 300U
#define MODE3_SAME_CELL_RETRIGGER_MS 700U
#define MODE3_RELEASE_CONFIRM_FRAMES 3U
#define MODE3_DUO_SAME_NOTE_MERGE_WINDOW_MS 80U
#define MODE3_FACE_LOST_GRACE_MS 500U
#define MODE3_CELL_HYSTERESIS 45U
#define MODE3_MAX_PLAYERS 2U

static const uint32_t s_note_colors[7] = {
    0x00D9FF, 0x4D8CFF, 0x8C5CFF, 0xFF5CC8, 0xFF7A59, 0xFFD166, 0x73E6A5,
};
static const lv_image_dsc_t *const s_mode1_note_images[7] = {
    &img_note_c, &img_note_d, &img_note_e, &img_note_f,
    &img_note_g, &img_note_a, &img_note_b,
};
static const lv_image_dsc_t *const s_mode2_note_images[3][7] = {
    {&note_lane1_star_40, &note_lane2_diamond_40, &note_lane3_circle_40,
     &note_lane4_star_purple_40, &note_lane5_heart_40, &note_lane6_hexagon_40,
     &note_lane1_star_40},
    {&note_lane1_star_52, &note_lane2_diamond_52, &note_lane3_circle_52,
     &note_lane4_star_purple_52, &note_lane5_heart_52, &note_lane6_hexagon_52,
     &note_lane1_star_52},
    {&note_lane1_star, &note_lane2_diamond, &note_lane3_circle,
     &note_lane4_star_purple, &note_lane5_heart, &note_lane6_hexagon,
     &note_lane1_star},
};
static const lv_image_dsc_t *const s_mode2_hit_rings[4] = {
    &hit_ring_perfect, &hit_ring_great, &hit_ring_good, &hit_ring_miss,
};
static const lv_image_dsc_t *const s_mode2_countdown_images[4] = {
    &countdown_3, &countdown_2, &countdown_1, &countdown_go,
};

static const char *TAG = "home";
typedef mode1_history_event_t mode1_record_item_t;

enum {
    MODE1_CONTROL_REC,
    MODE1_CONTROL_STOP,
    MODE1_CONTROL_PLAY,
    MODE1_CONTROL_LOOP,
};

enum {
    MODE2_CONTROL_START,
    MODE2_CONTROL_RESTART,
    MODE2_CONTROL_SPEED,
};

typedef struct {
    uint8_t id;
    const char *name;
    const uint8_t *notes;
    const uint32_t *times;
    uint8_t note_count;
    uint16_t beat_ms;
} mode2_song_t;

/* Future songs only need another notes array and a mode2_song_t descriptor. */
static const uint8_t s_mode2_twinkle_notes[] = {
    0, 0, 4, 4, 5, 5, 4,
    3, 3, 2, 2, 1, 1, 0,
    4, 4, 3, 3, 2, 2, 1,
    4, 4, 3, 3, 2, 2, 1,
    0, 0, 4, 4, 5, 5, 4,
    3, 3, 2, 2, 1, 1, 0,
};
static const mode2_song_t s_mode2_twinkle_song = {
    .id = 0,
    .name = "TWINKLE",
    .notes = s_mode2_twinkle_notes,
    .note_count = sizeof(s_mode2_twinkle_notes),
    .beat_ms = MODE2_BEAT_MS,
};
static const uint8_t s_mode2_ode_to_joy_notes[] = {
    2, 2, 3, 4, 4, 3, 2, 1,
    0, 0, 1, 2, 2, 1, 1,
    2, 2, 3, 4, 4, 3, 2, 1,
    0, 0, 1, 2, 1, 0, 0,
};
static const mode2_song_t s_mode2_ode_to_joy_song = {
    .id = 1,
    .name = "ODE_TO_JOY",
    .notes = s_mode2_ode_to_joy_notes,
    .note_count = sizeof(s_mode2_ode_to_joy_notes),
    .beat_ms = MODE2_BEAT_MS,
};
static const uint8_t s_mode2_mary_had_a_little_lamb_notes[] = {
    2, 1, 0, 1, 2, 2, 2,
    1, 1, 1, 2, 4, 4,
    2, 1, 0, 1, 2, 2, 2, 2,
    1, 1, 2, 1, 0,
};
static const mode2_song_t s_mode2_mary_had_a_little_lamb_song = {
    .id = 2,
    .name = "MARY_HAD_A_LITTLE_LAMB",
    .notes = s_mode2_mary_had_a_little_lamb_notes,
    .note_count = sizeof(s_mode2_mary_had_a_little_lamb_notes),
    .beat_ms = MODE2_BEAT_MS,
};
static const uint8_t s_mode2_jingle_bells_notes[] = {
    2, 2, 2, 2, 2, 2, 2, 4, 0, 1, 2,
    3, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 2, 1, 4,
};
static const mode2_song_t s_mode2_jingle_bells_song = {
    .id = 3,
    .name = "JINGLE_BELLS",
    .notes = s_mode2_jingle_bells_notes,
    .note_count = sizeof(s_mode2_jingle_bells_notes),
    .beat_ms = MODE2_BEAT_MS,
};
static const mode2_song_t *const s_mode2_songs[] = {
    &s_mode2_twinkle_song,
    &s_mode2_ode_to_joy_song,
    &s_mode2_mary_had_a_little_lamb_song,
    &s_mode2_jingle_bells_song,
};
static const mode2_song_t *s_mode2_song = &s_mode2_twinkle_song;
static mode2_song_t s_mode2_uploaded_song;
static uint8_t s_mode2_uploaded_notes[MODE2_USER_SONG_MAX_EVENTS];
static uint32_t s_mode2_uploaded_times[MODE2_USER_SONG_MAX_EVENTS];

static lv_obj_t *s_music_label;
static lv_obj_t *s_uart_label;
static lv_timer_t *s_uart_timer;
static bool s_mode1_active;
static bool s_mode2_active;
static lv_obj_t *s_mode1_status_label;
static lv_obj_t *s_mode1_mute_label;
static lv_obj_t *s_mode1_info_right_label;
static lv_obj_t *s_mode1_note_image;
static lv_obj_t *s_mode1_timeline_dots[MODE1_TIMELINE_STEPS];
static lv_obj_t *s_mode2_mute_label;
static lv_obj_t *s_mode2_cursor;
static lv_obj_t *s_mode2_note_cells[7];
static lv_obj_t *s_mode2_falling_notes[MODE2_SONG_MAX_NOTES];
static lv_timer_t *s_mode2_song_timer;
static lv_obj_t *s_mode2_speed_label;
static lv_obj_t *s_mode2_start_label;
static lv_obj_t *s_mode2_judge_image;
static lv_obj_t *s_mode2_hit_ring;
static lv_obj_t *s_mode2_hit_note;
static lv_obj_t *s_mode2_lane_flash;
static lv_obj_t *s_mode2_stats_label;
static lv_obj_t *s_mode2_combo_label;
static lv_obj_t *s_mode2_progress_bar;
static lv_obj_t *s_mode2_countdown_image;
static lv_obj_t *s_mode2_result_panel;
static lv_obj_t *s_mode2_result_label;
static lv_timer_t *s_mode2_countdown_timer;
static int s_mode2_lane = -1;
static int s_mode2_hand_lane = -1;
static int s_mode2_cursor_x = -1;
static uint8_t s_mode2_speed_index;
static uint32_t s_mode2_song_start_ms;
static uint32_t s_mode2_pause_start_ms;
static uint32_t s_mode2_score;
static uint16_t s_mode2_combo;
static uint16_t s_mode2_max_combo;
static uint16_t s_mode2_judgement_count[4];
static bool s_mode2_song_running;
static bool s_mode2_song_paused;
static uint8_t s_mode2_countdown_step;
static bool s_mode2_note_judged[MODE2_SONG_MAX_NOTES];
static uint8_t s_mode2_note_size_index[MODE2_SONG_MAX_NOTES];
static int s_mode1_last_action = -1;
static uint32_t s_mode1_last_note_ms;
static int s_mode1_candidate_action = -1;
static int s_mode1_candidate_band = -1;
static uint8_t s_mode1_candidate_frames;
static int s_mode1_stable_action = -1;
static int s_mode1_stable_band = -1;
static uint32_t s_mode1_lost_since_ms;
static uint32_t s_mode1_last_sequence;
static mode1_record_item_t s_mode1_record[MODE1_RECORD_MAX];
static uint8_t s_mode1_record_count;
static uint8_t s_mode1_play_index;
static uint8_t s_mode1_timeline_index;
static uint32_t s_mode1_record_start_ms;
static uint32_t s_mode1_play_start_ms;
static bool s_mode1_recording;
static bool s_mode1_playing;
static bool s_mode1_loop;
typedef struct {
    bool active;
    uint16_t x10;
    uint16_t y10;
    int band;
    int note;
    uint32_t last_seen_ms;
    uint32_t last_note_ms;
    int candidate_band;
    int candidate_note;
    int locked_band;
    int locked_note;
    uint8_t candidate_frames;
    uint8_t release_frames;
    bool has_triggered;
    bool pending_trigger;
} mode3_player_t;

static bool s_mode3_active;
static lv_obj_t *s_mode3_cells[3][7];
static lv_obj_t *s_mode3_cursors[MODE3_MAX_PLAYERS];
static lv_obj_t *s_mode3_mute_label;
static lv_obj_t *s_mode3_mode_label;
static mode3_player_t s_mode3_players[MODE3_MAX_PLAYERS];
static uint32_t s_mode3_cell_last_note_ms[3][7];
static uint8_t s_mode3_player_limit = 1;
static uint32_t s_mode3_trigger_count;

static void home_mode_clicked(lv_event_t *event);
static void mode1_record_start(void);
static void mode1_stop(void);
static void mode1_play_start(void);
static void mode1_loop_toggle(void);
static void mode2_song_start(void);
static void mode2_start_countdown(void);
static void mode2_update_stats(void);
static void mode2_song_tick(lv_timer_t *timer);
static void mode2_lane_position(int lane, int progress, int *x, int *y);
static void mode2_refresh_hand(void);
static const ai_uart_object_t *mode1_best_hand(const ai_uart_snapshot_t *snapshot);

uint8_t home_screen_mode2_song_count(void)
{
    return sizeof(s_mode2_songs) / sizeof(s_mode2_songs[0]) + mode2_user_song_count();
}

uint8_t home_screen_mode2_song_id_limit(void)
{
    return MODE2_USER_SONG_ID_BASE + MODE2_USER_SONG_MAX;
}

bool home_screen_mode2_song_get(uint8_t id, const char **name)
{
    if (id < sizeof(s_mode2_songs) / sizeof(s_mode2_songs[0])) {
        if (name) *name = s_mode2_songs[id]->name;
        return true;
    }
    const mode2_user_song_t *song = mode2_user_song_get(id - MODE2_USER_SONG_ID_BASE);
    if (!song) return false;
    if (name) *name = song->name;
    return true;
}

uint8_t home_screen_mode2_selected_song(void)
{
    return s_mode2_song->id;
}

bool home_screen_mode2_select_song(uint8_t id)
{
    if (s_mode2_song_running || s_mode2_countdown_timer) return false;
    if (id < sizeof(s_mode2_songs) / sizeof(s_mode2_songs[0])) {
        s_mode2_song = s_mode2_songs[id];
        return true;
    }
    const mode2_user_song_t *song = mode2_user_song_get(id - MODE2_USER_SONG_ID_BASE);
    if (!song) return false;
    for (uint8_t index = 0; index < song->event_count; index++) {
        s_mode2_uploaded_notes[index] = song->events[index].note;
        s_mode2_uploaded_times[index] = song->events[index].time_ms;
    }
    s_mode2_uploaded_song = (mode2_song_t){
        .id = id,
        .name = song->name,
        .notes = s_mode2_uploaded_notes,
        .times = s_mode2_uploaded_times,
        .note_count = song->event_count,
        .beat_ms = song->beat_ms,
    };
    s_mode2_song = &s_mode2_uploaded_song;
    return true;
}

bool home_screen_mode2_delete_song(uint8_t id)
{
    if (id < MODE2_USER_SONG_ID_BASE || s_mode2_song_running || s_mode2_countdown_timer) return false;
    if (!mode2_user_song_delete(id)) return false;
    if (s_mode2_song->id == id) s_mode2_song = &s_mode2_twinkle_song;
    return true;
}

static void mode3_apply_background(lv_obj_t *screen)
{
    /* Replace this fallback with lv_image_set_src(..., &mode3_bg) after adding a PNG asset. */
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x030817), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

/* ponytail: modules share private UI state through this unit; introduce a context object only if separate compilation is required. */
#include "home_mode1.c"
#include "home_mode2.c"
#include "home_mode3.c"

static void mode1_reset_trigger(void)
{
    s_mode1_last_action = -1;
    s_mode1_candidate_action = -1;
    s_mode1_candidate_band = -1;
    s_mode1_candidate_frames = 0;
    s_mode1_stable_action = -1;
    s_mode1_stable_band = -1;
    s_mode1_lost_since_ms = 0;
    s_mode1_last_sequence = UINT32_MAX;
    s_mode1_last_note_ms = 0;
}

static bool mode1_accept_observation(const ai_uart_object_t *hand,
                                     uint32_t sequence, uint32_t now)
{
    if (hand == NULL || hand->action < 0 || hand->action >= 7 ||
        hand->detect_confidence < MODE1_CONF_LOW_THRESHOLD) {
        if (s_mode1_stable_action >= 0 && s_mode1_lost_since_ms == 0) {
            s_mode1_lost_since_ms = now;
        }
        if (s_mode1_lost_since_ms != 0 &&
            now - s_mode1_lost_since_ms >= MODE1_GESTURE_LOST_GRACE_MS) {
            mode1_reset_trigger();
        }
        return false;
    }

    s_mode1_lost_since_ms = 0;
    if (sequence == s_mode1_last_sequence) return false;
    s_mode1_last_sequence = sequence;

    int band = 2 - hand->y10 * 3 / 1001;
    if (hand->action != s_mode1_candidate_action || band != s_mode1_candidate_band) {
        s_mode1_candidate_action = hand->action;
        s_mode1_candidate_band = band;
        s_mode1_candidate_frames = 1;
    } else if (s_mode1_candidate_frames < UINT8_MAX) {
        ++s_mode1_candidate_frames;
    }

    bool changed = hand->action != s_mode1_stable_action ||
                   band != s_mode1_stable_band;
    bool confirmed = hand->detect_confidence >= MODE1_CONF_HIGH_THRESHOLD ||
                     s_mode1_candidate_frames >= MODE1_CONFIRM_FRAMES;
    if (!changed || !confirmed) return false;
    if (s_mode1_last_note_ms != 0 &&
        now - s_mode1_last_note_ms < MODE1_NOTE_CHANGE_INTERVAL_MS) {
        return false;
    }
    s_mode1_stable_action = hand->action;
    s_mode1_stable_band = band;
    s_mode1_last_action = hand->action;
    s_mode1_last_note_ms = now;
    return true;
}

static void home_uart_refresh(lv_timer_t *timer)
{
    (void)timer;
    if (s_uart_label == NULL) {
        return;
    }
    if (s_mode1_active && s_mode1_playing) {
        mode1_playback_update(lv_tick_get());
        return;
    }
    ai_uart_snapshot_t snapshot;
    if (!ai_uart_service_get_snapshot(&snapshot)) {
        if (s_mode3_active) mode3_clear_players();
        if (s_mode1_active) mode1_set_info("-", "-", -1, s_mode1_recording ? "RECORDING" : "SEARCHING");
        else if (s_mode2_active) {
        if (s_mode2_cursor) lv_obj_add_flag(s_mode2_cursor, LV_OBJ_FLAG_HIDDEN);
        s_mode2_cursor_x = -1;
            s_mode2_hand_lane = -1;
            mode2_set_note_highlight(-1);
        }
        else lv_label_set_text(s_uart_label, s_mode3_active ? "FACE SEARCHING\nUART WAITING" : "UART WAITING");
        return;
    }
    if (s_mode1_active) {
        const ai_uart_object_t *hand = snapshot.mode == AI_UART_MODE_GEST ? mode1_best_hand(&snapshot) : NULL;
        if (hand == NULL) {
            (void)mode1_accept_observation(NULL, snapshot.sequence, lv_tick_get());
            if (s_mode1_note_image) lv_obj_add_flag(s_mode1_note_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(s_uart_label, lv_color_hex(0x66F2FF), 0);
            mode1_set_info("-", "-", -1, s_mode1_recording ? "RECORDING" : "SEARCHING");
        } else {
            static const char *const notes[7] = {"do", "re", "mi", "fa", "sol", "la", "si"};
            static const char *const bands[3] = {"LOW", "MID", "HIGH"};
            int band = 2 - hand->y10 * 3 / 1001;
            uint32_t note_color = hand->action >= 0 && hand->action < 7 ?
                                  s_note_colors[hand->action] : 0x66F2FF;
            if (mode1_accept_observation(hand, snapshot.sequence, lv_tick_get())) {
                note_audio_play(band, hand->action);
                mode1_record_note(band, hand->action, hand->x10, hand->y10, lv_tick_get());
                mode1_timeline_advance(hand->action);
                s_mode1_last_action = hand->action;
                s_mode1_last_note_ms = lv_tick_get();
            }
            if (hand->action >= 0 && hand->action < 7) {
                mode1_show_note_icon(hand->action, hand->x10, hand->y10);
            } else if (s_mode1_note_image) {
                lv_obj_add_flag(s_mode1_note_image, LV_OBJ_FLAG_HIDDEN);
            }
            lv_obj_set_style_text_color(s_uart_label, lv_color_hex(note_color), 0);
            mode1_set_info(hand->action >= 0 && hand->action < 7 ? notes[hand->action] : "-",
                           bands[band], hand->action, s_mode1_recording ? "RECORDING" : "LIVE");
        }
        return;
    }
    if (s_mode2_active) {
        mode2_refresh_hand();
        return;
    }
    if (s_mode3_active) {
        mode3_refresh_faces(&snapshot);
        return;
    }
    lv_label_set_text_fmt(s_uart_label, "UART %s  SEQ %lu\nOBJECTS %u",
                          ai_uart_mode_name(snapshot.mode),
                          (unsigned long)snapshot.sequence,
                          (unsigned)snapshot.object_count);
}

static void home_music_clicked(lv_event_t *event)
{
    (void)event;
    bool enabled = !audio_service_home_bgm_enabled();
    if (audio_service_set_home_bgm_enabled(enabled) && s_music_label) {
        lv_label_set_text(s_music_label, enabled ? "M" : "X");
    }
}

static void home_add_music_button(lv_obj_t *parent)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 70, 70);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -30, 30);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x66F2FF), 0);
    lv_obj_add_event_cb(button, home_music_clicked, LV_EVENT_CLICKED, NULL);

    s_music_label = lv_label_create(button);
    lv_label_set_text(s_music_label, audio_service_home_bgm_enabled() ? "M" : "X");
    lv_obj_set_style_text_color(s_music_label, lv_color_white(), 0);
    lv_obj_center(s_music_label);
}

static void home_show_mode(const char *mode_name)
{
    (void)audio_service_set_home_bgm_enabled(false);
    if (s_mode2_countdown_timer) {
        lv_timer_delete(s_mode2_countdown_timer);
        s_mode2_countdown_timer = NULL;
    }

    ai_uart_mode_t uart_mode = AI_UART_MODE_GEST;
    uint32_t interval_ms = MODE1_UART_POLL_MS;
    if (mode_name[5] == '2') {
        uart_mode = AI_UART_MODE_HAND;
        interval_ms = MODE2_UART_POLL_MS;
    } else if (mode_name[5] == '3') {
        uart_mode = AI_UART_MODE_FACE;
        interval_ms = MODE3_UART_POLL_MS;
    }
    ai_uart_service_select_mode(uart_mode, interval_ms);
    if (s_uart_timer) lv_timer_set_period(s_uart_timer, mode_name[5] == '2' ? 1000U : interval_ms);
    s_mode1_active = mode_name[5] == '1';
    if (s_mode1_active) {
        note_audio_set_enabled(true);
    }
    mode1_reset_trigger();
    s_mode3_active = mode_name[5] == '3';
    s_mode2_active = mode_name[5] == '2';

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, 800, 1280);
    mode3_apply_background(screen);
    if (s_mode1_active) {
        lv_obj_t *background = lv_image_create(screen);
        lv_image_set_src(background, &free_mode_bg_2);
        lv_obj_set_pos(background, 0, 0);
        lv_obj_clear_flag(background, LV_OBJ_FLAG_CLICKABLE);
    }
    if (s_mode2_active) {
        lv_obj_t *background = lv_image_create(screen);
        lv_image_set_src(background, &practice_bg);
        lv_obj_set_pos(background, 0, 0);
        lv_obj_clear_flag(background, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, s_mode3_active ? "FACE MUSIC" : mode_name);
    lv_obj_set_width(label, s_mode3_active ? 800 : LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    if (s_mode3_active) {
        lv_obj_set_pos(label, 0, 90);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_t *subtitle = lv_label_create(screen);
        lv_label_set_text(subtitle, "FACE POSITION -> NOTE");
        lv_obj_set_width(subtitle, 800);
        lv_obj_set_pos(subtitle, 0, 125);
        lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9CEBFF), 0);
    } else if (!s_mode2_active) {
        lv_obj_set_pos(label, 350, 180);
    } else {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *home_button = lv_button_create(screen);
    lv_obj_set_pos(home_button, 30, 30);
    lv_obj_set_size(home_button, 110, 55);
    lv_obj_add_event_cb(home_button, home_mode_clicked, LV_EVENT_CLICKED, "HOME");
    lv_obj_t *home_label = lv_label_create(home_button);
    lv_label_set_text(home_label, "HOME");
    lv_obj_center(home_label);









    if (s_mode1_active) home_mode1_create(screen);
    if (s_mode2_active) home_mode2_create(screen);
    if (s_mode3_active) {
        home_mode3_create(screen);
        home_mode3_grid_create(screen);
    }

    s_uart_label = lv_label_create(screen);
    lv_obj_null_on_delete(&s_uart_label);
    lv_obj_set_width(s_uart_label, s_mode1_active ? 470 : 760);
    lv_obj_set_pos(s_uart_label, s_mode3_active ? 20 : s_mode1_active ? 90 : 20,
                   s_mode3_active ? 1080 : s_mode1_active ? 980 : 760);
    lv_obj_set_style_text_align(s_uart_label, s_mode1_active ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_uart_label, lv_color_hex(0x66F2FF), 0);
    if (s_mode1_active) lv_obj_set_style_text_line_space(s_uart_label, 22, 0);
    if (s_mode2_active) lv_obj_add_flag(s_uart_label, LV_OBJ_FLAG_HIDDEN);
    home_uart_refresh(NULL);

    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

static void home_mode_clicked(lv_event_t *event)
{
    const char *mode_name = lv_event_get_user_data(event);
    if (mode_name[0] == 'H') {
        home_screen_create();
        return;
    }
    ESP_LOGI(TAG, "Selected %s", mode_name);
    home_show_mode(mode_name);
}

static void home_add_mode_button(lv_obj_t *parent,
                                 lv_coord_t x,
                                 lv_coord_t y,
                                 lv_coord_t width,
                                 lv_coord_t height,
                                 const char *name)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x66F2FF), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(button, 1, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, home_mode_clicked, LV_EVENT_CLICKED, (void *)name);
}

void home_screen_create(void)
{
    s_mode1_active = false;
    s_mode2_active = false;
    s_mode1_status_label = NULL;
    s_mode1_mute_label = NULL;
    s_mode1_info_right_label = NULL;
    s_mode1_note_image = NULL;
    s_mode2_mute_label = NULL;
    s_mode2_cursor = NULL;
    s_mode2_speed_label = NULL;
    s_mode2_judge_image = NULL;
    s_mode2_hit_ring = NULL;
    s_mode2_hit_note = NULL;
    s_mode2_lane_flash = NULL;
    s_mode2_stats_label = NULL;
    s_mode2_lane = -1;
    s_mode2_hand_lane = -1;
    s_mode2_cursor_x = -1;
    s_mode2_song_running = false;
    for (int i = 0; i < 7; i++) s_mode2_note_cells[i] = NULL;
    for (int i = 0; i < MODE2_SONG_MAX_NOTES; i++) s_mode2_falling_notes[i] = NULL;
    for (int i = 0; i < MODE1_TIMELINE_STEPS; i++) s_mode1_timeline_dots[i] = NULL;
    s_mode1_recording = false;
    s_mode1_playing = false;
    s_mode3_active = false;
    for (int i = 0; i < MODE3_MAX_PLAYERS; i++) s_mode3_cursors[i] = NULL;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, 800, 1280);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *background = lv_image_create(screen);
    lv_image_set_src(background, &hom);
    lv_obj_set_pos(background, 0, 0);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_CLICKABLE);

    (void)audio_service_set_home_bgm_enabled(true);
    home_add_music_button(screen);

    home_add_mode_button(screen, 105, 375, 590, 155, "MODE 1");
    home_add_mode_button(screen, 105, 555, 590, 155, "MODE 2");
    home_add_mode_button(screen, 105, 735, 590, 155, "MODE 3");

    if (s_uart_timer == NULL) {
        s_uart_timer = lv_timer_create(home_uart_refresh, 100, NULL);
    } else {
        lv_timer_set_period(s_uart_timer, 100);
    }
}
