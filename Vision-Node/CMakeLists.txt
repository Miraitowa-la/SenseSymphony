# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "components/Middlewares/face_detect_middleware"
    "components/Middlewares/hand_detect_middleware"
    "components/Middlewares/ai_uart_comm"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(00_basic)
