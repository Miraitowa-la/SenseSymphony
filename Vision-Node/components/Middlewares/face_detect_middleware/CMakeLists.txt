idf_component_register(
    SRCS "face_detect_middleware.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp-dl human_face_detect
)
