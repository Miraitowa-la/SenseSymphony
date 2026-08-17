# Slave: camera + LCD + FACE/EXPR/HAND/GEST detect + UART response.
idf_component_register(SRCS "main_slave.c"
                    INCLUDE_DIRS "."
                    REQUIRES BSP driver esp_timer ai_uart_comm face_detect_middleware hand_detect_middleware)

# Master: UART query/switch controller.
# idf_component_register(SRCS "main_master.c"
#                     INCLUDE_DIRS "."
#                     REQUIRES driver ai_uart_comm)
