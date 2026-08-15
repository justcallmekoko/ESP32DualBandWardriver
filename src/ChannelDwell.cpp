#include "ChannelDwell.h"

#include <limits.h>

uint16_t calculateSoloDwellMs(uint16_t base_dwell_ms,
                              uint16_t channel_popularity,
                              uint16_t peak_popularity,
                              uint8_t maximum_modifier) {
  if (base_dwell_ms == 0)
    return 0;

  if ((channel_popularity == 0) || (peak_popularity == 0) ||
      (maximum_modifier <= 1))
    return base_dwell_ms;

  const uint32_t extra_range =
    static_cast<uint32_t>(base_dwell_ms) * (maximum_modifier - 1);
  const uint32_t weighted_extra =
    (extra_range * channel_popularity) / peak_popularity;
  const uint32_t dwell_ms = static_cast<uint32_t>(base_dwell_ms) + weighted_extra;

  return dwell_ms > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(dwell_ms);
}
