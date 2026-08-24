static void mode3_mute_clicked(lv_event_t *event);
static void mode3_mode_clicked(lv_event_t *event);
static void mode3_cell_clicked(lv_event_t *event);
static void mode3_clear_players(void);
static void mode3_refresh_faces(const ai_uart_snapshot_t *snapshot);
static void mode3_refresh_single(const ai_uart_snapshot_t *snapshot);

#define MODE3_FACE_MIN_CONFIDENCE 100U

static const char *mode3_mode_name(void)
{
    return s_mode3_player_limit == 1 ? "SINGLE" : "DUO";
}

static void home_mode3_create(lv_obj_t *screen)
{
    lv_obj_t *mute_button = lv_button_create(screen);
    lv_obj_set_pos(mute_button, 650, 30);
    lv_obj_set_size(mute_button, 120, 55);
    lv_obj_add_event_cb(mute_button, mode3_mute_clicked, LV_EVENT_CLICKED, NULL);
    s_mode3_mute_label = lv_label_create(mute_button);
    lv_label_set_text(s_mode3_mute_label, note_audio_enabled() ? "SOUND" : "MUTE");
    lv_obj_center(s_mode3_mute_label);

    lv_obj_t *mode_button = lv_button_create(screen);
    lv_obj_set_pos(mode_button, 260, 1170);
    lv_obj_set_size(mode_button, 280, 60);
    lv_obj_add_event_cb(mode_button, mode3_mode_clicked, LV_EVENT_CLICKED, NULL);
    s_mode3_mode_label = lv_label_create(mode_button);
    lv_label_set_text_fmt(s_mode3_mode_label, "MODE: %s", mode3_mode_name());
    lv_obj_center(s_mode3_mode_label);
}

static void home_mode3_grid_create(lv_obj_t *screen)
{
    static const char *const bands[3] = {"LOW", "MID", "HIGH"};
    static const char *const notes[7] = {"do", "re", "mi", "fa", "sol", "la", "si"};
    static const uint32_t colors[7] = {0x00D9FF, 0x4D8CFF, 0x8C5CFF, 0xFF5CC8, 0xFF7A59, 0xFFD166, 0x73E6A5};
    static const uint32_t player_colors[MODE3_MAX_PLAYERS] = {0xFFFFFF, 0xFFD166};
    const int grid_x = 60, grid_y = 260, grid_w = 700, grid_h = 760;
    const int cell_w = grid_w / 7, cell_h = grid_h / 3;
    memset(s_mode3_players, 0, sizeof(s_mode3_players));
    for (int player = 0; player < MODE3_MAX_PLAYERS; ++player) {
        s_mode3_players[player].band = -1;
        s_mode3_players[player].note = -1;
        s_mode3_players[player].candidate_band = -1;
        s_mode3_players[player].candidate_note = -1;
        s_mode3_players[player].locked_band = -1;
        s_mode3_players[player].locked_note = -1;
    }
    memset(s_mode3_cell_last_note_ms, 0, sizeof(s_mode3_cell_last_note_ms));
    s_mode3_trigger_count = 0;
    for (int band = 0; band < 3; band++) for (int note = 0; note < 7; note++) {
        lv_obj_t *cell = lv_obj_create(screen);
        s_mode3_cells[band][note] = cell;
        lv_obj_set_pos(cell, grid_x + note * cell_w, grid_y + (2 - band) * cell_h);
        lv_obj_set_size(cell, note == 6 ? grid_w - note * cell_w : cell_w, cell_h);
        lv_obj_set_style_radius(cell, 0, 0);
        lv_obj_set_style_bg_color(cell, lv_color_hex(colors[note]), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_20, 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_set_style_border_color(cell, lv_color_hex(0xC7E9FF), 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(cell, mode3_cell_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)(band * 7 + note));
        lv_obj_t *cell_label = lv_label_create(cell);
        lv_label_set_text_fmt(cell_label, "%s\n%s", bands[band], notes[note]);
        lv_obj_set_style_text_align(cell_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(cell_label, lv_color_white(), 0);
        lv_obj_center(cell_label);
    }
    for (int player = 0; player < MODE3_MAX_PLAYERS; player++) {
        lv_obj_t *cursor = lv_obj_create(screen);
        s_mode3_cursors[player] = cursor;
        lv_obj_remove_style_all(cursor);
        lv_obj_set_size(cursor, 20, 20);
        lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cursor, lv_color_hex(player_colors[player]), 0);
        lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cursor, 3, 0);
        lv_obj_set_style_border_color(cursor, lv_color_hex(0x030817), 0);
        lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
    }
}

static void mode3_mute_clicked(lv_event_t *event)
{
    (void)event;
    note_audio_set_enabled(!note_audio_enabled());
    lv_label_set_text(s_mode3_mute_label, note_audio_enabled() ? "SOUND" : "MUTE");
}

static void mode3_mode_clicked(lv_event_t *event)
{
    (void)event;
    s_mode3_player_limit = s_mode3_player_limit == 1 ? 2 : 1;
    mode3_clear_players();
    lv_label_set_text_fmt(s_mode3_mode_label, "MODE: %s", mode3_mode_name());
}

static void mode3_cell_clicked(lv_event_t *event)
{
    uintptr_t id = (uintptr_t)lv_event_get_user_data(event);
    note_audio_play(id / 7, id % 7);
}

static int mode3_note_from_x(const mode3_player_t *player, uint16_t x)
{
    int note = x * 7 / 1001;
    if (player->active && player->note >= 0 && note != player->note) {
        unsigned boundary = (unsigned)(note > player->note ? player->note + 1 : player->note) * 1000U / 7U;
        unsigned distance = x > boundary ? x - boundary : boundary - x;
        if (distance < MODE3_CELL_HYSTERESIS) note = player->note;
    }
    return note;
}

static int mode3_band_from_y(const mode3_player_t *player, uint16_t y)
{
    int row = y * 3 / 1001;
    int old_row = player->band < 0 ? -1 : 2 - player->band;
    if (player->active && old_row >= 0 && row != old_row) {
        unsigned boundary = (unsigned)(row > old_row ? old_row + 1 : old_row) * 1000U / 3U;
        unsigned distance = y > boundary ? y - boundary : boundary - y;
        if (distance < MODE3_CELL_HYSTERESIS) row = old_row;
    }
    return 2 - row;
}

static void mode3_refresh_highlights(void)
{
    for (int band = 0; band < 3; band++) for (int note = 0; note < 7; note++) {
        lv_obj_set_style_bg_opa(s_mode3_cells[band][note], LV_OPA_20, 0);
        lv_obj_set_style_border_width(s_mode3_cells[band][note], 1, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(s_mode3_cells[band][note], 0), lv_color_white(), 0);
    }
    for (int player = 0; player < s_mode3_player_limit; player++) if (s_mode3_players[player].active) {
        mode3_player_t *state = &s_mode3_players[player];
        lv_obj_set_style_bg_opa(s_mode3_cells[state->band][state->note], LV_OPA_70, 0);
        lv_obj_set_style_border_width(s_mode3_cells[state->band][state->note], 4, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(s_mode3_cells[state->band][state->note], 0), lv_color_black(), 0);
    }
}

static void mode3_clear_players(void)
{
    memset(s_mode3_players, 0, sizeof(s_mode3_players));
    for (int player = 0; player < MODE3_MAX_PLAYERS; ++player) {
        s_mode3_players[player].band = -1;
        s_mode3_players[player].note = -1;
        s_mode3_players[player].candidate_band = -1;
        s_mode3_players[player].candidate_note = -1;
        s_mode3_players[player].locked_band = -1;
        s_mode3_players[player].locked_note = -1;
    }
    for (int player = 0; player < MODE3_MAX_PLAYERS; player++) if (s_mode3_cursors[player]) lv_obj_add_flag(s_mode3_cursors[player], LV_OBJ_FLAG_HIDDEN);
    if (s_mode3_cells[0][0]) mode3_refresh_highlights();
}

static bool mode3_update_trigger(mode3_player_t *state, bool valid,
                                 uint32_t now)
{
    if (!valid) {
        if (state->release_frames < MODE3_RELEASE_CONFIRM_FRAMES) {
            ++state->release_frames;
        }
        if (state->release_frames == MODE3_RELEASE_CONFIRM_FRAMES) {
            state->locked_band = -1;
            state->locked_note = -1;
            state->candidate_band = -1;
            state->candidate_note = -1;
            state->candidate_frames = 0;
            state->has_triggered = false;
            state->pending_trigger = false;
        }
        return false;
    }

    state->release_frames = 0;
    if (state->band != state->locked_band || state->note != state->locked_note) {
        if (state->band != state->candidate_band || state->note != state->candidate_note) {
            state->candidate_band = state->band;
            state->candidate_note = state->note;
            state->candidate_frames = 1;
        } else if (state->candidate_frames < UINT8_MAX) {
            ++state->candidate_frames;
        }
        if (state->candidate_frames < MODE3_DEBOUNCE_FRAMES) return false;
        state->locked_band = state->band;
        state->locked_note = state->note;
        state->candidate_frames = 0;
        if (!state->has_triggered ||
            now - state->last_note_ms >= MODE3_NOTE_CHANGE_MIN_GAP_MS) {
            state->has_triggered = true;
            state->last_note_ms = now;
            return true;
        }
        state->pending_trigger = true;
        return false;
    }
    if (state->pending_trigger &&
        now - state->last_note_ms >= MODE3_NOTE_CHANGE_MIN_GAP_MS) {
        state->pending_trigger = false;
        state->last_note_ms = now;
        return true;
    }
    if (state->has_triggered &&
        now - state->last_note_ms >= MODE3_SAME_CELL_RETRIGGER_MS) {
        state->last_note_ms = now;
        return true;
    }
    return false;
}

static void mode3_hide_player(int player)
{
    mode3_player_t *state = &s_mode3_players[player];
    if (state->active && lv_tick_elaps(state->last_seen_ms) > MODE3_FACE_LOST_GRACE_MS) {
        state->active = false;
        (void)mode3_update_trigger(state, false, lv_tick_get());
        lv_obj_add_flag(s_mode3_cursors[player], LV_OBJ_FLAG_HIDDEN);
    }
}

static void mode3_observe_player(int player, const ai_uart_object_t *face,
                                 uint32_t now)
{
    mode3_player_t *state = &s_mode3_players[player];
    state->x10 = face->x10;
    state->y10 = face->y10;
    state->band = mode3_band_from_y(state, face->y10);
    state->note = mode3_note_from_x(state, face->x10);
    state->last_seen_ms = now;
    state->active = true;
    lv_obj_set_pos(s_mode3_cursors[player], 50 + face->x10 * 700 / 1000,
                   250 + face->y10 * 760 / 1000);
    lv_obj_clear_flag(s_mode3_cursors[player], LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_mode3_cursors[player]);
}

static void mode3_emit_if_ready(int player, uint32_t now)
{
    mode3_player_t *state = &s_mode3_players[player];
    if (!mode3_update_trigger(state, state->active, now)) return;
    if (s_mode3_player_limit == 2 &&
        now - s_mode3_cell_last_note_ms[state->locked_band][state->locked_note] <
            MODE3_DUO_SAME_NOTE_MERGE_WINDOW_MS) {
        return;
    }
    note_audio_play(state->locked_band, state->locked_note);
    s_mode3_cell_last_note_ms[state->locked_band][state->locked_note] = now;
    ++s_mode3_trigger_count;
}

static void mode3_refresh_single(const ai_uart_snapshot_t *snapshot)
{
    if (snapshot->mode != AI_UART_MODE_FACE || snapshot->object_count == 0) {
        mode3_hide_player(0);
        mode3_emit_if_ready(0, lv_tick_get());
        mode3_refresh_highlights();
        return;
    }

    const ai_uart_object_t *face = &snapshot->objects[0];
    uint32_t now = lv_tick_get();
    mode3_observe_player(0, face, now);
    mode3_emit_if_ready(0, now);
    mode3_refresh_highlights();
    lv_label_set_text_fmt(s_uart_label, "SINGLE  FACE 1  CONF %u%%\nAUDIO %s  TRIGGERS %lu",
                          face->detect_confidence / 10, note_audio_enabled() ? "ON" : "OFF",
                          (unsigned long)s_mode3_trigger_count);
}

static void mode3_refresh_faces(const ai_uart_snapshot_t *snapshot)
{
    if (s_mode3_player_limit == 1) {
        mode3_refresh_single(snapshot);
        return;
    }
    bool used[AI_UART_MAX_OBJECTS] = {0};
    uint32_t now = lv_tick_get();
    uint8_t locked = 0;
    for (int player = 0; player < s_mode3_player_limit; player++) {
        int best = -1;
        uint32_t best_distance = UINT32_MAX;
        if (s_mode3_players[player].active) for (size_t i = 0; i < snapshot->object_count; i++) {
            const ai_uart_object_t *face = &snapshot->objects[i];
            if (used[i] || face->detect_confidence < MODE3_FACE_MIN_CONFIDENCE) continue;
            int dx = (int)face->x10 - s_mode3_players[player].x10;
            int dy = (int)face->y10 - s_mode3_players[player].y10;
            uint32_t distance = (uint32_t)(dx * dx + dy * dy);
            if (distance < best_distance) { best = (int)i; best_distance = distance; }
        }
        if (best < 0) for (size_t i = 0; i < snapshot->object_count; i++) {
            if (used[i] || snapshot->objects[i].detect_confidence < MODE3_FACE_MIN_CONFIDENCE) continue;
            if (best < 0 || snapshot->objects[i].detect_confidence > snapshot->objects[best].detect_confidence) best = (int)i;
        }
        mode3_player_t *state = &s_mode3_players[player];
        if (best < 0) {
            mode3_hide_player(player);
            mode3_emit_if_ready(player, now);
            continue;
        }
        used[best] = true;
        const ai_uart_object_t *face = &snapshot->objects[best];
        mode3_observe_player(player, face, now);
        locked++;
        mode3_emit_if_ready(player, now);
    }
    mode3_refresh_highlights();
    lv_label_set_text_fmt(s_uart_label, "%s  FACES %u  LOCK %u/%u\nCONF %u%%  AUDIO %s  TRIGGERS %lu",
                          mode3_mode_name(), (unsigned)snapshot->object_count, locked,
                          (unsigned)s_mode3_player_limit,
                          snapshot->object_count ? snapshot->objects[0].detect_confidence / 10 : 0,
                          note_audio_enabled() ? "ON" : "OFF", (unsigned long)s_mode3_trigger_count);
}
