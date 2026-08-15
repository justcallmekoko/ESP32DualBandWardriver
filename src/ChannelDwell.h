#pragma once

#include <stdint.h>

uint16_t calculateSoloDwellMs(uint16_t base_dwell_ms,
                              uint16_t channel_popularity,
                              uint16_t peak_popularity,
                              uint8_t maximum_modifier = 3);
