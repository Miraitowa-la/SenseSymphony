idf_component_register(
    SRCS "hand_detect_middleware.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp-dl hand_detect hand_gesture_recognition
)
