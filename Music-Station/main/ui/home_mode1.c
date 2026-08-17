static void mode1_mute_clicked(lv_event_t *event);
static void mode1_add_control(lv_obj_t *parent, int x, int y, const char *icon, const char *text, uint32_t id);
static void mode1_add_zone_label(lv_obj_t *parent, int y, const char *name, uint32_t color);

static void home_mode1_create(lv_obj_t *screen)
{
        lv_obj_t *mute_button = lv_button_create(screen);
        lv_obj_set_pos(mute_button, 650, 30);
        lv_obj_set_size(mute_button, 120, 55);
        lv_obj_add_event_cb(mute_button, mode1_mute_clicked, LV_EVENT_CLICKED, NULL);
        s_mode1_mute_label = lv_label_create(mute_button);
        lv_label_set_text(s_mode1_mute_label, note_audio_enabled() ? "SOUND" : "MUTE");
        lv_obj_center(s_mode1_mute_label);
        lv_obj_t *subtitle = lv_label_create(screen);
        lv_label_set_text(subtitle, "SINGLE HAND FREE PLAY");
        lv_obj_set_width(subtitle, 800);
        lv_obj_set_pos(subtitle, 0, 140);
        lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9CEBFF), 0);
        lv_obj_t *timeline_label = lv_label_create(screen);
        lv_label_set_text(timeline_label, "MELODY TIMELINE");
        lv_obj_set_width(timeline_label, 800);
        lv_obj_set_pos(timeline_label, 0, 340);
        lv_obj_set_style_text_align(timeline_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(timeline_label, lv_color_hex(0x66F2FF), 0);
        for (int i = 0; i < MODE1_TIMELINE_STEPS; i++) {
            s_mode1_timeline_dots[i] = lv_obj_create(screen);
            lv_obj_remove_style_all(s_mode1_timeline_dots[i]);
            lv_obj_set_size(s_mode1_timeline_dots[i], 18, 18);
            lv_obj_set_pos(s_mode1_timeline_dots[i], 150 + i * 42, 365);
            lv_obj_set_style_radius(s_mode1_timeline_dots[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_mode1_timeline_dots[i], lv_color_hex(0x66F2FF), 0);
            lv_obj_set_style_bg_opa(s_mode1_timeline_dots[i], LV_OPA_20, 0);
            lv_obj_set_style_border_width(s_mode1_timeline_dots[i], 2, 0);
            lv_obj_set_style_border_color(s_mode1_timeline_dots[i], lv_color_hex(0xBDEFFF), 0);
        }
        s_mode1_timeline_index = 0;
        s_mode1_note_image = lv_image_create(screen);
        lv_image_set_src(s_mode1_note_image, &img_note_c);
        lv_obj_clear_flag(s_mode1_note_image, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_mode1_note_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_mode1_note_image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *info_panel = lv_obj_create(screen);
        lv_obj_set_pos(info_panel, 45, 940);
        lv_obj_set_size(info_panel, 710, 250);
        lv_obj_set_style_radius(info_panel, 18, 0);
        lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x020D1D), 0);
        lv_obj_set_style_bg_opa(info_panel, LV_OPA_80, 0);
        lv_obj_set_style_border_width(info_panel, 2, 0);
        lv_obj_set_style_border_color(info_panel, lv_color_hex(0x00D9FF), 0);
        lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);
        s_mode1_status_label = lv_label_create(screen);
        lv_label_set_text(s_mode1_status_label, "READY");
        lv_obj_set_width(s_mode1_status_label, 430);
        lv_obj_set_pos(s_mode1_status_label, 90, 1145);
        lv_obj_set_style_text_align(s_mode1_status_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_align(s_mode1_status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_mode1_status_label, lv_color_hex(0xFFD166), 0);
        s_mode1_info_right_label = lv_label_create(screen);
        lv_obj_set_width(s_mode1_info_right_label, 180);
        lv_obj_set_pos(s_mode1_info_right_label, 350, 985);
        lv_obj_set_style_text_color(s_mode1_info_right_label, lv_color_hex(0x66F2FF), 0);
        lv_obj_set_style_text_line_space(s_mode1_info_right_label, 22, 0);
        lv_obj_t *note_range = lv_obj_create(screen);
        lv_obj_set_pos(note_range, 90, 410);
        lv_obj_set_size(note_range, 620, 330);
        lv_obj_set_style_radius(note_range, 12, 0);
        lv_obj_set_style_bg_opa(note_range, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(note_range, 3, 0);
        lv_obj_set_style_border_color(note_range, lv_color_hex(0xBDEFFF), 0);
        lv_obj_clear_flag(note_range, LV_OBJ_FLAG_SCROLLABLE);
        mode1_add_zone_label(screen, 445, "HIGH", 0x8C5CFF);
        mode1_add_zone_label(screen, 555, "MID", 0x00D9FF);
        mode1_add_zone_label(screen, 665, "LOW", 0xFF7A59);
        mode1_add_control(screen, 585, 980, "\xE2\x97\x8F", "REC", MODE1_CONTROL_REC);
        mode1_add_control(screen, 670, 980, LV_SYMBOL_STOP, "STOP", MODE1_CONTROL_STOP);
        mode1_add_control(screen, 585, 1075, LV_SYMBOL_PLAY, "PLAY", MODE1_CONTROL_PLAY);
        mode1_add_control(screen, 670, 1075, LV_SYMBOL_LOOP, "LOOP", MODE1_CONTROL_LOOP);
    }

static void mode1_control_clicked(lv_event_t *event)
{
    switch ((uintptr_t)lv_event_get_user_data(event)) {
    case MODE1_CONTROL_REC: mode1_record_start(); break;
    case MODE1_CONTROL_STOP: mode1_stop(); break;
    case MODE1_CONTROL_PLAY: mode1_play_start(); break;
    case MODE1_CONTROL_LOOP: mode1_loop_toggle(); break;
    }
}

static void mode1_mute_clicked(lv_event_t *event)
{
    (void)event;
    note_audio_set_enabled(!note_audio_enabled());
    lv_label_set_text(s_mode1_mute_label, note_audio_enabled() ? "SOUND" : "MUTE");
}

static void mode1_add_control(lv_obj_t *parent, int x, int y, const char *icon, const char *text, uint32_t id)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 70, 65);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x10264B), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x66F2FF), 0);
    lv_obj_add_event_cb(button, mode1_control_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)id);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text_fmt(label, "%s\n%s", icon, text);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
}

static void mode1_add_zone_label(lv_obj_t *parent, int y, const char *name, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, name);
    lv_obj_set_pos(label, 105, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
}

static void mode1_set_info(const char *note, const char *zone, int gesture, const char *state)
{
    lv_label_set_text_fmt(s_uart_label, "NOTE   %s\nZONE   %s\nTONE   GESTURE %d", note, zone, gesture);
    lv_label_set_text_fmt(s_mode1_info_right_label, "VOL    100%%\nTEMPO  300ms\nSTATE  %s", state);
}

static void mode1_set_note_icon(int note)
{
    if (note >= 0 && note < 7 && s_mode1_note_image) {
        lv_image_set_src(s_mode1_note_image, s_mode1_note_images[note]);
    }
}

static void mode1_show_note_icon(int note, uint16_t x10, uint16_t y10)
{
    if (note < 0 || note >= 7 || s_mode1_note_image == NULL) return;
    mode1_set_note_icon(note);
    lv_obj_set_pos(s_mode1_note_image, 68 + x10 * 592 / 1000,
                   406 + y10 * 302 / 1000);
    lv_obj_clear_flag(s_mode1_note_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_mode1_note_image);
}

static void mode1_timeline_advance(uint8_t note)
{
    lv_obj_t *dot = s_mode1_timeline_dots[s_mode1_timeline_index];
    if (dot == NULL) return;
    lv_obj_set_style_bg_color(dot, lv_color_hex(s_note_colors[note]), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    s_mode1_timeline_index = (s_mode1_timeline_index + 1) % MODE1_TIMELINE_STEPS;
}

static void mode1_record_note(uint8_t band, uint8_t note, uint16_t x10, uint16_t y10, uint32_t now)
{
    if (!s_mode1_recording || s_mode1_record_count >= MODE1_RECORD_MAX) return;
    s_mode1_record[s_mode1_record_count++] = (mode1_record_item_t) {
        .band = band, .note = note, .x10 = x10, .y10 = y10,
        .time_ms = now - s_mode1_record_start_ms,
    };
    lv_label_set_text_fmt(s_mode1_status_label, "RECORDING  %u/%u",
                          (unsigned)s_mode1_record_count, MODE1_RECORD_MAX);
}

static void mode1_record_start(void)
{
    s_mode1_playing = false;
    s_mode1_recording = true;
    s_mode1_record_count = 0;
    s_mode1_record_start_ms = lv_tick_get();
    s_mode1_timeline_index = 0;
    for (int i = 0; i < MODE1_TIMELINE_STEPS; i++) {
        lv_obj_set_style_bg_opa(s_mode1_timeline_dots[i], LV_OPA_20, 0);
    }
    lv_label_set_text(s_mode1_status_label, "RECORDING  0/32");
}

static void mode1_stop(void)
{
    bool was_recording = s_mode1_recording;
    s_mode1_recording = false;
    s_mode1_playing = false;
    bool saved = was_recording && mode1_history_save(s_mode1_record, s_mode1_record_count);
    lv_label_set_text_fmt(s_mode1_status_label, saved ? "SAVED  %u NOTES" : "STOP  %u NOTES", (unsigned)s_mode1_record_count);
}

static void mode1_play_start(void)
{
    if (s_mode1_record_count == 0) {
        lv_label_set_text(s_mode1_status_label, "NO RECORDING");
        return;
    }
    s_mode1_recording = false;
    s_mode1_playing = true;
    s_mode1_play_index = 0;
    s_mode1_play_start_ms = lv_tick_get();
    lv_label_set_text_fmt(s_mode1_status_label, "PLAYING  %u NOTES", (unsigned)s_mode1_record_count);
}

static void mode1_loop_toggle(void)
{
    s_mode1_loop = !s_mode1_loop;
    lv_label_set_text(s_mode1_status_label, s_mode1_loop ? "LOOP ON" : "LOOP OFF");
}

static void mode1_playback_update(uint32_t now)
{
    static const char *const notes[7] = {"do", "re", "mi", "fa", "sol", "la", "si"};
    static const char *const bands[3] = {"LOW", "MID", "HIGH"};
    while (s_mode1_playing && s_mode1_play_index < s_mode1_record_count &&
           now - s_mode1_play_start_ms >= s_mode1_record[s_mode1_play_index].time_ms) {
        const mode1_record_item_t *item = &s_mode1_record[s_mode1_play_index++];
        note_audio_play(item->band, item->note);
        mode1_show_note_icon(item->note, item->x10, item->y10);
        lv_obj_set_style_text_color(s_uart_label, lv_color_hex(s_note_colors[item->note]), 0);
        mode1_set_info(notes[item->note], bands[item->band], item->note, "PLAYING");
        mode1_timeline_advance(item->note);
    }
    if (s_mode1_playing && s_mode1_play_index == s_mode1_record_count) {
        if (s_mode1_loop) {
            s_mode1_play_index = 0;
            s_mode1_play_start_ms = now;
        } else {
            s_mode1_playing = false;
            lv_label_set_text(s_mode1_status_label, "PLAY COMPLETE");
        }
    }
}

static const ai_uart_object_t *mode1_best_hand(const ai_uart_snapshot_t *snapshot)
{
    const ai_uart_object_t *best = NULL;
    for (size_t i = 0; i < snapshot->object_count; i++) {
        if (best == NULL || snapshot->objects[i].detect_confidence > best->detect_confidence) {
            best = &snapshot->objects[i];
        }
    }
    return best;
}

