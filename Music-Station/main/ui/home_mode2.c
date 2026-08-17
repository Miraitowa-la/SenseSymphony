static void mode2_mute_clicked(lv_event_t *event);
static void mode2_add_control(lv_obj_t *parent, int x, const char *text, uint32_t id);
static void mode2_add_result_control(lv_obj_t *parent, int x, const char *text, uintptr_t id);

static void home_mode2_create(lv_obj_t *screen)
{
        lv_obj_t *mute_button = lv_button_create(screen);
        lv_obj_set_pos(mute_button, 650, 30);
        lv_obj_set_size(mute_button, 120, 55);
        lv_obj_add_event_cb(mute_button, mode2_mute_clicked, LV_EVENT_CLICKED, NULL);
        s_mode2_mute_label = lv_label_create(mute_button);
        if (note_audio_enabled()) {
            lv_label_set_text_fmt(s_mode2_mute_label, "VOL %u", note_audio_volume());
        } else {
            lv_label_set_text(s_mode2_mute_label, "MUTE");
        }
        lv_obj_center(s_mode2_mute_label);
        s_mode2_cursor = lv_obj_create(screen);
        lv_obj_remove_style_all(s_mode2_cursor);
        lv_obj_set_size(s_mode2_cursor, 24, 24);
        lv_obj_set_style_radius(s_mode2_cursor, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_mode2_cursor, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_bg_opa(s_mode2_cursor, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_mode2_cursor, 3, 0);
        lv_obj_set_style_border_color(s_mode2_cursor, lv_color_white(), 0);
        lv_obj_set_style_shadow_width(s_mode2_cursor, 14, 0);
        lv_obj_set_style_shadow_color(s_mode2_cursor, lv_color_hex(0x00E5FF), 0);
        lv_obj_add_flag(s_mode2_cursor, LV_OBJ_FLAG_HIDDEN);
        s_mode2_lane = -1;
        s_mode2_cursor_x = -1;
        static const char *const notes[7] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si"};
        for (int lane = 0; lane < 7; lane++) {
            lv_obj_t *cell = lv_obj_create(screen);
            s_mode2_note_cells[lane] = cell;
            lv_obj_set_pos(cell, 45 + lane * 102, 972);
            lv_obj_set_size(cell, 82, 42);
            lv_obj_set_style_radius(cell, 18, 0);
            lv_obj_set_style_bg_color(cell, lv_color_hex(s_note_colors[lane]), 0);
            lv_obj_set_style_bg_opa(cell, LV_OPA_20, 0);
            lv_obj_set_style_border_width(cell, 1, 0);
            lv_obj_set_style_border_color(cell, lv_color_hex(s_note_colors[lane]), 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t *cell_label = lv_label_create(cell);
            lv_label_set_text(cell_label, notes[lane]);
            lv_obj_set_style_text_color(cell_label, lv_color_hex(s_note_colors[lane]), 0);
            lv_obj_center(cell_label);
        }
        for (int i = 0; i < s_mode2_song->note_count; i++) {
            lv_obj_t *note = lv_image_create(screen);
            s_mode2_falling_notes[i] = note;
            lv_image_set_src(note, s_mode2_note_images[2][s_mode2_song->notes[i]]);
            lv_obj_set_size(note, 64, 64);
            lv_obj_clear_flag(note, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(note, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(note, LV_OBJ_FLAG_HIDDEN);
        }
        s_mode2_hit_ring = lv_image_create(screen);
        lv_obj_clear_flag(s_mode2_hit_ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_mode2_hit_ring, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_mode2_hit_ring, LV_OBJ_FLAG_HIDDEN);
        s_mode2_lane_flash = lv_obj_create(screen);
        lv_obj_set_size(s_mode2_lane_flash, 88, 8);
        lv_obj_set_style_radius(s_mode2_lane_flash, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_mode2_lane_flash, 0, 0);
        lv_obj_clear_flag(s_mode2_lane_flash, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_mode2_lane_flash, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_mode2_lane_flash, LV_OBJ_FLAG_HIDDEN);
        s_mode2_hit_note = lv_image_create(screen);
        lv_obj_clear_flag(s_mode2_hit_note, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_mode2_hit_note, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_mode2_hit_note, LV_OBJ_FLAG_HIDDEN);
        s_mode2_judge_image = lv_image_create(screen);
        lv_obj_clear_flag(s_mode2_judge_image, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_mode2_judge_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_mode2_judge_image, LV_OBJ_FLAG_HIDDEN);
        s_mode2_combo_label = lv_label_create(screen);
        lv_obj_set_pos(s_mode2_combo_label, 590, 135);
        lv_obj_set_width(s_mode2_combo_label, 160);
        lv_obj_set_style_text_align(s_mode2_combo_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_mode2_combo_label, lv_color_hex(0xFFD166), 0);
        lv_label_set_text(s_mode2_combo_label, "COMBO\n000");
        s_mode2_progress_bar = lv_bar_create(screen);
        lv_obj_set_pos(s_mode2_progress_bar, 110, 1065);
        lv_obj_set_size(s_mode2_progress_bar, 580, 12);
        lv_bar_set_range(s_mode2_progress_bar, 0, 100);
        lv_bar_set_value(s_mode2_progress_bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_mode2_progress_bar, lv_color_hex(0x071326), 0);
        lv_obj_set_style_bg_opa(s_mode2_progress_bar, LV_OPA_80, 0);
        lv_obj_set_style_radius(s_mode2_progress_bar, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_mode2_progress_bar, lv_color_hex(0xA767FF), LV_PART_INDICATOR);
        lv_obj_set_style_radius(s_mode2_progress_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        s_mode2_stats_label = lv_label_create(screen);
        lv_obj_set_pos(s_mode2_stats_label, 65, 1088);
        lv_obj_set_width(s_mode2_stats_label, 670);
        lv_obj_set_style_text_align(s_mode2_stats_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_mode2_stats_label, lv_color_hex(0xC6F6FF), 0);
        lv_obj_set_style_text_line_space(s_mode2_stats_label, 8, 0);
        lv_label_set_text(s_mode2_stats_label, "SCORE 000000   COMBO 000\nMAX 000   ACC 100%   PROG 000%");
        s_mode2_countdown_image = lv_image_create(screen);
        lv_obj_clear_flag(s_mode2_countdown_image, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_mode2_countdown_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_mode2_countdown_image, LV_OBJ_FLAG_HIDDEN);
        s_mode2_result_panel = lv_obj_create(screen);
        lv_obj_set_pos(s_mode2_result_panel, 155, 525);
        lv_obj_set_size(s_mode2_result_panel, 490, 230);
        lv_obj_set_style_radius(s_mode2_result_panel, 26, 0);
        lv_obj_set_style_bg_color(s_mode2_result_panel, lv_color_hex(0x061225), 0);
        lv_obj_set_style_bg_opa(s_mode2_result_panel, LV_OPA_90, 0);
        lv_obj_set_style_border_width(s_mode2_result_panel, 3, 0);
        lv_obj_set_style_border_color(s_mode2_result_panel, lv_color_hex(0xA767FF), 0);
        lv_obj_clear_flag(s_mode2_result_panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(s_mode2_result_panel, 0, 0);
        s_mode2_result_label = lv_label_create(s_mode2_result_panel);
        lv_obj_set_size(s_mode2_result_label, 490, 160);
        lv_obj_set_pos(s_mode2_result_label, 0, 12);
        lv_obj_set_style_text_align(s_mode2_result_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_mode2_result_label, lv_color_hex(0xE6F7FF), 0);
        lv_obj_set_style_text_line_space(s_mode2_result_label, 4, 0);
        mode2_add_result_control(s_mode2_result_panel, 74, "RETRY", 0);
        mode2_add_result_control(s_mode2_result_panel, 256, "HOME", 1);
        lv_obj_add_flag(s_mode2_result_panel, LV_OBJ_FLAG_HIDDEN);
        s_mode2_song_running = false;
        s_mode2_song_paused = false;
        if (s_mode2_song_timer == NULL) {
            s_mode2_song_timer = lv_timer_create(mode2_song_tick, MODE2_SONG_TICK_MS, NULL);
        }
        mode2_add_control(screen, 100, "START", MODE2_CONTROL_START);
        mode2_add_control(screen, 325, "RESTART", MODE2_CONTROL_RESTART);
        mode2_add_control(screen, 550, "SPEED 0.5X", MODE2_CONTROL_SPEED);
    }

static void mode2_mute_clicked(lv_event_t *event)
{
    (void)event;
    if (!note_audio_enabled()) {
        note_audio_set_enabled(true);
        note_audio_set_volume(30);
    } else if (note_audio_volume() < 60) {
        note_audio_set_volume(60);
    } else if (note_audio_volume() < 100) {
        note_audio_set_volume(100);
    } else {
        note_audio_set_enabled(false);
    }
    if (note_audio_enabled()) {
        lv_label_set_text_fmt(s_mode2_mute_label, "VOL %u", note_audio_volume());
    } else {
        lv_label_set_text(s_mode2_mute_label, "MUTE");
    }
}

static void mode2_control_clicked(lv_event_t *event)
{
    uintptr_t id = (uintptr_t)lv_event_get_user_data(event);
    if (id == MODE2_CONTROL_START) {
        if (s_mode2_countdown_timer) return;
        if (!s_mode2_song_running) {
            mode2_start_countdown();
        } else {
            s_mode2_song_paused = !s_mode2_song_paused;
            if (s_mode2_song_paused) {
                s_mode2_pause_start_ms = lv_tick_get();
            } else {
                s_mode2_song_start_ms += lv_tick_elaps(s_mode2_pause_start_ms);
            }
            lv_label_set_text(s_mode2_start_label, s_mode2_song_paused ? "RESUME" : "PAUSE");
        }
    } else if (id == MODE2_CONTROL_RESTART) {
        mode2_start_countdown();
    } else if (id == MODE2_CONTROL_SPEED) {
        static const char *const speed_names[] = {"SPEED 0.5X", "SPEED 0.75X", "SPEED 1.0X", "SPEED 1.25X"};
        s_mode2_speed_index = (s_mode2_speed_index + 1) % 4;
        lv_label_set_text(s_mode2_speed_label, speed_names[s_mode2_speed_index]);
    }
}

static void mode2_add_control(lv_obj_t *parent, int x, const char *text, uint32_t id)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, 1148);
    lv_obj_set_size(button, 150, 64);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x101538), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_80, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0xA767FF), 0);
    lv_obj_add_event_cb(button, mode2_control_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)id);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    if (id == MODE2_CONTROL_SPEED) s_mode2_speed_label = label;
    if (id == MODE2_CONTROL_START) s_mode2_start_label = label;
}

static void mode2_result_clicked(lv_event_t *event)
{
    if ((uintptr_t)lv_event_get_user_data(event) == 0) mode2_start_countdown();
    else home_screen_create();
}

static void mode2_add_result_control(lv_obj_t *parent, int x, const char *text, uintptr_t id)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, 174);
    lv_obj_set_size(button, 160, 42);
    lv_obj_set_style_radius(button, 16, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x172047), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_90, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0xA767FF), 0);
    lv_obj_add_event_cb(button, mode2_result_clicked, LV_EVENT_CLICKED, (void *)id);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
}

static void mode2_set_note_highlight(int lane)
{
    if (lane == s_mode2_lane) return;
    if (s_mode2_lane >= 0) {
        lv_obj_t *old_cell = s_mode2_note_cells[s_mode2_lane];
        lv_obj_set_style_bg_opa(old_cell, LV_OPA_20, 0);
        lv_obj_set_style_border_width(old_cell, 1, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(old_cell, 0),
                                    lv_color_hex(s_note_colors[s_mode2_lane]), 0);
    }
    s_mode2_lane = lane;
    if (lane >= 0) {
        lv_obj_t *cell = s_mode2_note_cells[lane];
        lv_obj_set_style_bg_opa(cell, LV_OPA_70, 0);
        lv_obj_set_style_border_width(cell, 3, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(cell, 0), lv_color_black(), 0);
    }
}

static uint32_t mode2_note_time(int index)
{
    return s_mode2_song->times ? s_mode2_song->times[index] : (uint32_t)index * s_mode2_song->beat_ms;
}

static void mode2_song_start(void)
{
    s_mode2_song_start_ms = lv_tick_get();
    s_mode2_song_running = true;
    s_mode2_song_paused = false;
    s_mode2_score = 0;
    s_mode2_combo = 0;
    s_mode2_max_combo = 0;
    for (int i = 0; i < 4; i++) s_mode2_judgement_count[i] = 0;
    lv_label_set_text(s_mode2_start_label, "PAUSE");
    lv_obj_add_flag(s_mode2_result_panel, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < s_mode2_song->note_count; i++) {
        s_mode2_note_judged[i] = false;
        s_mode2_note_size_index[i] = 0xff;
        lv_obj_add_flag(s_mode2_falling_notes[i], LV_OBJ_FLAG_HIDDEN);
    }
    mode2_update_stats();
}

static uint32_t mode2_accuracy(void)
{
    uint16_t judged = s_mode2_judgement_count[0] + s_mode2_judgement_count[1] +
                       s_mode2_judgement_count[2] + s_mode2_judgement_count[3];
    return judged ? (s_mode2_score * 100U + judged * 500U) / (judged * 1000U) : 100U;
}

static void mode2_countdown_tick(lv_timer_t *timer)
{
    if (!s_mode2_active) {
        lv_timer_delete(timer);
        s_mode2_countdown_timer = NULL;
        return;
    }
    if (++s_mode2_countdown_step == 4) {
        lv_timer_delete(timer);
        s_mode2_countdown_timer = NULL;
        lv_obj_add_flag(s_mode2_countdown_image, LV_OBJ_FLAG_HIDDEN);
        mode2_song_start();
        return;
    }
    const lv_image_dsc_t *image = s_mode2_countdown_images[s_mode2_countdown_step];
    lv_image_set_src(s_mode2_countdown_image, image);
    lv_obj_set_size(s_mode2_countdown_image, image->header.w, image->header.h);
    lv_obj_set_pos(s_mode2_countdown_image, 400 - image->header.w / 2, 600 - image->header.h / 2);
}

static void mode2_start_countdown(void)
{
    if (s_mode2_countdown_timer) {
        lv_timer_delete(s_mode2_countdown_timer);
        s_mode2_countdown_timer = NULL;
    }
    s_mode2_song_running = false;
    s_mode2_song_paused = false;
    s_mode2_countdown_step = 0;
    s_mode2_score = 0;
    s_mode2_combo = 0;
    s_mode2_max_combo = 0;
    for (int i = 0; i < 4; i++) s_mode2_judgement_count[i] = 0;
    for (int i = 0; i < s_mode2_song->note_count; i++) {
        s_mode2_note_judged[i] = false;
        s_mode2_note_size_index[i] = 0xff;
        lv_obj_add_flag(s_mode2_falling_notes[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s_mode2_judge_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mode2_hit_ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mode2_hit_note, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mode2_lane_flash, LV_OBJ_FLAG_HIDDEN);
    mode2_update_stats();
    lv_obj_add_flag(s_mode2_result_panel, LV_OBJ_FLAG_HIDDEN);
    const lv_image_dsc_t *image = s_mode2_countdown_images[0];
    lv_image_set_src(s_mode2_countdown_image, image);
    lv_obj_set_size(s_mode2_countdown_image, image->header.w, image->header.h);
    lv_obj_set_pos(s_mode2_countdown_image, 400 - image->header.w / 2, 600 - image->header.h / 2);
    lv_obj_clear_flag(s_mode2_countdown_image, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_mode2_start_label, "READY");
    s_mode2_countdown_timer = lv_timer_create(mode2_countdown_tick, MODE2_COUNTDOWN_MS, NULL);
}

static void mode2_judge_fade(void *obj, int32_t value)
{
    lv_obj_set_style_opa(obj, value, 0);
}

static void mode2_show_hit_ring(int lane, int result)
{
    lv_obj_t *ring = s_mode2_hit_ring;
    const lv_image_dsc_t *source = s_mode2_hit_rings[result];
    int x, y;
    mode2_lane_position(lane, 1000, &x, &y);
    lv_anim_delete(ring, NULL);
    lv_image_set_src(ring, source);
    lv_obj_set_size(ring, source->header.w, source->header.h);
    lv_image_set_pivot(ring, source->header.w / 2, source->header.h / 2);
    lv_obj_set_style_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_pos(ring, x - source->header.w / 2, y - source->header.h / 2);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ring);
    lv_anim_set_exec_cb(&anim, mode2_judge_fade);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, 250);
    lv_anim_start(&anim);
}

static void mode2_show_hit_note(int lane)
{
    const lv_image_dsc_t *source = s_mode2_note_images[2][lane];
    int x, y;
    mode2_lane_position(lane, 1000, &x, &y);
    lv_anim_delete(s_mode2_hit_note, NULL);
    lv_image_set_src(s_mode2_hit_note, source);
    lv_obj_set_size(s_mode2_hit_note, source->header.w, source->header.h);
    lv_image_set_pivot(s_mode2_hit_note, source->header.w / 2, source->header.h / 2);
    lv_obj_set_style_opa(s_mode2_hit_note, LV_OPA_COVER, 0);
    lv_obj_set_pos(s_mode2_hit_note, x - source->header.w / 2, y - source->header.h / 2);
    lv_obj_clear_flag(s_mode2_hit_note, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_mode2_hit_note);
    lv_anim_set_exec_cb(&anim, mode2_judge_fade);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, 220);
    lv_anim_start(&anim);
}

static void mode2_show_lane_flash(int lane)
{
    int x, y;
    mode2_lane_position(lane, 1000, &x, &y);
    lv_anim_delete(s_mode2_lane_flash, NULL);
    lv_obj_set_pos(s_mode2_lane_flash, x - 44, y - 4);
    lv_obj_set_style_bg_color(s_mode2_lane_flash, lv_color_hex(s_note_colors[lane]), 0);
    lv_obj_set_style_bg_opa(s_mode2_lane_flash, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_mode2_lane_flash, LV_OBJ_FLAG_HIDDEN);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_mode2_lane_flash);
    lv_anim_set_exec_cb(&anim, mode2_judge_fade);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, 180);
    lv_anim_start(&anim);
}

static void mode2_show_judge(int result)
{
    static const lv_image_dsc_t *const images[] = {
        &judge_perfect, &judge_great, &judge_good, &judge_miss,
    };
    lv_obj_t *image = s_mode2_judge_image;
    lv_anim_delete(image, NULL);
    lv_image_set_src(image, images[result]);
    lv_obj_set_size(image, 430, 166);
    lv_obj_set_pos(image, 185, 597);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(image, LV_OPA_COVER, 0);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, image);
    lv_anim_set_exec_cb(&anim, mode2_judge_fade);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_delay(&anim, 380);
    lv_anim_set_duration(&anim, 120);
    lv_anim_start(&anim);
}

static void mode2_update_stats(void)
{
    uint16_t judged = s_mode2_judgement_count[0] + s_mode2_judgement_count[1] +
                       s_mode2_judgement_count[2] + s_mode2_judgement_count[3];
    uint32_t accuracy = mode2_accuracy();
    uint32_t progress = judged * 100U / s_mode2_song->note_count;
    lv_label_set_text_fmt(s_mode2_stats_label, "SCORE %06lu   COMBO %03u\nMAX %03u   ACC %03lu%%   PROG %03lu%%",
                          (unsigned long)s_mode2_score, s_mode2_combo, s_mode2_max_combo,
                          (unsigned long)accuracy, (unsigned long)progress);
    lv_bar_set_value(s_mode2_progress_bar, progress, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_mode2_combo_label, "COMBO\n%03u", s_mode2_combo);
}

static void mode2_apply_judgement(int result)
{
    static const uint16_t score[] = {1000, 700, 400, 0};
    s_mode2_judgement_count[result]++;
    if (result == 3) {
        s_mode2_combo = 0;
    } else {
        s_mode2_score += score[result];
        s_mode2_combo++;
        if (s_mode2_combo > s_mode2_max_combo) s_mode2_max_combo = s_mode2_combo;
    }
    mode2_update_stats();
}

static void mode2_show_result(void)
{
    const mode2_history_result_t result = {
        .song_id = s_mode2_song->id,
        .score = s_mode2_score,
        .max_combo = s_mode2_max_combo,
        .perfect = s_mode2_judgement_count[0],
        .great = s_mode2_judgement_count[1],
        .good = s_mode2_judgement_count[2],
        .miss = s_mode2_judgement_count[3],
        .accuracy = (uint8_t)mode2_accuracy(),
    };
    (void)mode2_history_save(&result);
    lv_label_set_text_fmt(s_mode2_result_label,
                          "RESULT\nSCORE  %06lu\nPERFECT  %u    GREAT  %u\nGOOD  %u    MISS  %u\nMAX COMBO  %u    ACC  %lu%%",
                          (unsigned long)s_mode2_score,
                          s_mode2_judgement_count[0], s_mode2_judgement_count[1],
                          s_mode2_judgement_count[2], s_mode2_judgement_count[3],
                          s_mode2_max_combo, (unsigned long)mode2_accuracy());
    lv_obj_clear_flag(s_mode2_result_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_mode2_result_panel);
    lv_label_set_text(s_mode2_start_label, "START");
}

static void mode2_refresh_hand(void)
{
    ai_uart_snapshot_t snapshot;
    const ai_uart_object_t *hand = ai_uart_service_get_snapshot(&snapshot) &&
                                       snapshot.mode == AI_UART_MODE_HAND ?
                                   mode1_best_hand(&snapshot) : NULL;
    if (hand == NULL) {
        if (s_mode2_cursor) lv_obj_add_flag(s_mode2_cursor, LV_OBJ_FLAG_HIDDEN);
        s_mode2_hand_lane = -1;
        mode2_set_note_highlight(-1);
        return;
    }
    int lane = hand->x10 * 7 / 1001;
    int pointer_x = MODE2_LANE_JUDGE_LEFT_X +
                    hand->x10 * (MODE2_LANE_JUDGE_RIGHT_X - MODE2_LANE_JUDGE_LEFT_X) / 1000;
    s_mode2_hand_lane = lane;
    if (lv_obj_has_flag(s_mode2_cursor, LV_OBJ_FLAG_HIDDEN) || pointer_x != s_mode2_cursor_x) {
        s_mode2_cursor_x = pointer_x;
        lv_obj_set_pos(s_mode2_cursor, pointer_x - 12, MODE2_LANE_JUDGE_Y - 12);
        lv_obj_clear_flag(s_mode2_cursor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_mode2_cursor);
    }
    mode2_set_note_highlight(lane);
}

static void mode2_lane_position(int lane, int progress, int *x, int *y)
{
    int ratio = 2 * lane + 1;
    int x_top = MODE2_LANE_TOP_LEFT_X +
                (MODE2_LANE_TOP_RIGHT_X - MODE2_LANE_TOP_LEFT_X) * ratio / 14;
    int x_bottom = MODE2_LANE_JUDGE_LEFT_X +
                   (MODE2_LANE_JUDGE_RIGHT_X - MODE2_LANE_JUDGE_LEFT_X) * ratio / 14;
    *x = x_top + (x_bottom - x_top) * progress / 1000;
    *y = MODE2_LANE_TOP_Y + (MODE2_LANE_JUDGE_Y - MODE2_LANE_TOP_Y) * progress / 1000;
}

static void mode2_song_tick(lv_timer_t *timer)
{
    (void)timer;
    if (!s_mode2_active) return;
    mode2_refresh_hand();
    if (!s_mode2_song_running || s_mode2_song_paused) return;

    static const uint8_t speed_percent[] = {50, 75, 100, 125};
    uint32_t elapsed = lv_tick_elaps(s_mode2_song_start_ms) * speed_percent[s_mode2_speed_index] / 100;
    for (int i = 0; i < s_mode2_song->note_count; i++) {
        uint32_t spawn_ms = mode2_note_time(i);
        if (elapsed < spawn_ms || elapsed - spawn_ms > MODE2_NOTE_FALL_MS) {
            lv_obj_add_flag(s_mode2_falling_notes[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        uint32_t age = elapsed - spawn_ms;
        int lane = s_mode2_song->notes[i];
        uint32_t distance = age > MODE2_NOTE_FALL_MS ? age - MODE2_NOTE_FALL_MS : MODE2_NOTE_FALL_MS - age;
        if (!s_mode2_note_judged[i] && distance <= MODE2_GOOD_MS &&
            s_mode2_hand_lane >= lane - 1 && s_mode2_hand_lane <= lane + 1) {
            int result = distance <= MODE2_PERFECT_MS ? 0 :
                         distance <= MODE2_GREAT_MS ? 1 : 2;
            mode2_show_judge(result);
            mode2_show_hit_ring(lane, result);
            mode2_show_hit_note(lane);
            mode2_show_lane_flash(lane);
            note_audio_play(1, lane);
            mode2_apply_judgement(result);
            s_mode2_note_judged[i] = true;
            lv_obj_add_flag(s_mode2_falling_notes[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        if (!s_mode2_note_judged[i] && age > MODE2_NOTE_FALL_MS + MODE2_GOOD_MS) {
            mode2_show_judge(3);
            mode2_show_hit_ring(lane, 3);
            mode2_show_lane_flash(lane);
            mode2_apply_judgement(3);
            s_mode2_note_judged[i] = true;
            lv_obj_add_flag(s_mode2_falling_notes[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        if (s_mode2_note_judged[i]) continue;
        int progress = (int)(age * 1000U / MODE2_NOTE_FALL_MS);
        int x, y;
        mode2_lane_position(lane, progress, &x, &y);
        lv_obj_t *note = s_mode2_falling_notes[i];
        uint8_t size_index = progress < 333 ? 0 : progress < 666 ? 1 : 2;
        const lv_image_dsc_t *source = s_mode2_note_images[size_index][lane];
        if (s_mode2_note_size_index[i] != size_index) {
            s_mode2_note_size_index[i] = size_index;
            lv_image_set_src(note, source);
            lv_obj_set_size(note, source->header.w, source->header.h);
        }
        lv_obj_set_pos(note, x - source->header.w / 2, y - source->header.h / 2);
        lv_obj_set_style_opa(note, age < 220 ? LV_OPA_60 : LV_OPA_COVER, 0);
        lv_obj_clear_flag(note, LV_OBJ_FLAG_HIDDEN);
    }
    if (elapsed > mode2_note_time(s_mode2_song->note_count - 1) +
                  MODE2_NOTE_FALL_MS + MODE2_GOOD_MS) {
        s_mode2_song_running = false;
        mode2_show_result();
    }
}

