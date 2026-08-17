file(GLOB MODE3_AUDIO_FILES "assets/audio/mode3_face_notes/*.wav")
file(GLOB MODE1_NOTE_FILES "assets/image/mode1/c/img_note_*.c")
file(GLOB MODE2_IMAGE_FILES "assets/image/mode2/c/*.c")

idf_component_register(SRCS "main.c" "ui/home_screen.c" "audio/audio_service.c" "audio/note_audio_service.c" "uart/ai_uart_service.c" "uart/app_uart_service.c" "storage/mode1_history_service.c" "storage/mode2_history_service.c" "storage/mode2_user_song_service.c" "assets/image/home/c/home_bg.c" "assets/image/mode1/c/free_mode_bg_2.c" ${MODE1_NOTE_FILES} ${MODE2_IMAGE_FILES}
                    INCLUDE_DIRS "."
                    REQUIRES BSP lvgl__lvgl esp_timer ai_uart_comm driver spiffs
                    EMBED_FILES "assets/audio/home_bgm.wav" ${MODE3_AUDIO_FILES})
