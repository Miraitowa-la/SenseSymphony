#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ai_uart_protocol.h"
#include "esp_err.h"

esp_err_t ai_uart_service_start(void);
void ai_uart_service_select_mode(ai_uart_mode_t mode, uint32_t poll_interval_ms);
bool ai_uart_service_get_snapshot(ai_uart_snapshot_t *snapshot);
