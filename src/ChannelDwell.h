#pragma once

#include <stdint.h>

uint16_t updateSoloYieldEma(uint16_t previous_yield_per_second,
                            uint16_t new_unique_networks,
                            uint32_t scan_duration_ms);

uint8_t selectSoloBonusChannels(const uint16_t* yields_per_second,
                                uint8_t channel_count,
                                uint8_t* selected_channels,
                                uint8_t maximum_selected);

uint8_t calculatePopularityBarHeight(uint16_t channel_popularity,
                                     uint16_t peak_popularity,
                                     uint8_t maximum_height);
