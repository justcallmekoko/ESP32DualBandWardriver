#include "ChannelDwell.h"

#include <limits.h>

uint16_t updateSoloYieldEma(uint16_t previous_yield_per_second,
                            uint16_t new_unique_networks,
                            uint32_t scan_duration_ms) {
  if (scan_duration_ms == 0)
    return 0;

  // Normalize the sample to new unique BSSIDs per second so channels that
  // terminate at the active-scan minimum are compared fairly with channels
  // that remain until the maximum.
  const uint64_t bounded_sample =
    static_cast<uint64_t>(new_unique_networks) * 1000;
  const uint32_t sample = bounded_sample / scan_duration_ms > UINT16_MAX
    ? UINT16_MAX
    : static_cast<uint32_t>(bounded_sample / scan_duration_ms);

  // One part new sample and three parts history avoids reshuffling the bonus
  // channels because of a single unusually busy scan.
  const uint32_t ema =
    (static_cast<uint32_t>(previous_yield_per_second) * 3 + sample + 2) / 4;
  return ema > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(ema);
}

uint8_t selectSoloBonusChannels(const uint16_t* yields_per_second,
                                uint8_t channel_count,
                                uint8_t* selected_channels,
                                uint8_t maximum_selected) {
  if ((yields_per_second == nullptr) || (selected_channels == nullptr))
    return 0;

  uint8_t selected_count = 0;
  for (uint8_t channel = 0; channel < channel_count; channel++) {
    const uint16_t score = yields_per_second[channel];
    if (score == 0)
      continue;

    uint8_t insert_at = selected_count;
    while (insert_at > 0 &&
           score > yields_per_second[selected_channels[insert_at - 1]]) {
      insert_at--;
    }

    if (insert_at >= maximum_selected)
      continue;

    const uint8_t new_count = selected_count < maximum_selected
      ? selected_count + 1
      : selected_count;
    for (uint8_t i = new_count; i > insert_at + 1; i--)
      selected_channels[i - 1] = selected_channels[i - 2];

    selected_channels[insert_at] = channel;
    selected_count = new_count;
  }

  return selected_count;
}

uint8_t calculatePopularityBarHeight(uint16_t channel_popularity,
                                     uint16_t peak_popularity,
                                     uint8_t maximum_height) {
  if ((channel_popularity == 0) || (peak_popularity == 0) ||
      (maximum_height == 0))
    return 0;

  const uint32_t scaled =
    (static_cast<uint32_t>(channel_popularity) * maximum_height) /
    peak_popularity;
  if (scaled == 0)
    return 1;
  return scaled > maximum_height ? maximum_height : static_cast<uint8_t>(scaled);
}
