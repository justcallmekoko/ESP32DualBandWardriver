#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ChannelDwell.h"
#include "WigleFormat.h"
#include "CoreBinding.h"

int main() {
  assert(updateSoloYieldEma(0, 8, 1000) == 2);
  assert(updateSoloYieldEma(8, 8, 1000) == 8);
  assert(updateSoloYieldEma(16, 0, 1000) == 12);
  assert(updateSoloYieldEma(123, 10, 0) == 0);
  assert(updateSoloYieldEma(UINT16_MAX, UINT16_MAX, 1) == UINT16_MAX);

  const uint16_t yields[] = {0, 20, 5, 20, 9, 1};
  uint8_t selected[3] = {0};
  assert(selectSoloBonusChannels(yields, 6, selected, 3) == 3);
  assert(selected[0] == 1);
  assert(selected[1] == 3);
  assert(selected[2] == 4);

  uint8_t all_selected[8] = {0};
  assert(selectSoloBonusChannels(yields, 6, all_selected, 8) == 5);
  assert(selectSoloBonusChannels(nullptr, 6, all_selected, 8) == 0);
  assert(selectSoloBonusChannels(yields, 6, nullptr, 8) == 0);
  assert(selectSoloBonusChannels(yields, 6, all_selected, 0) == 0);

  assert(calculatePopularityBarHeight(0, 10, 48) == 0);
  assert(calculatePopularityBarHeight(5, 10, 48) == 24);
  assert(calculatePopularityBarHeight(1, 100, 48) == 1);
  assert(calculatePopularityBarHeight(10, 10, 48) == 48);
  assert(calculatePopularityBarHeight(20, 10, 48) == 48);
  assert(calculatePopularityBarHeight(5, 0, 48) == 0);
  assert(calculatePopularityBarHeight(5, 10, 0) == 0);

  size_t columns = 1;
  for (const char* cursor = WIGLE_CSV_COLUMNS; *cursor; ++cursor)
    if (*cursor == ',') ++columns;
  assert(columns == 14);
  assert(strstr(WIGLE_CSV_COLUMNS, "Frequency") != nullptr);
  assert(strstr(WIGLE_CSV_COLUMNS, "RCOIs,MfgrId,Type") != nullptr);
  assert(wigleFrequencyMHz(1) == 2412);
  assert(wigleFrequencyMHz(13) == 2472);
  assert(wigleFrequencyMHz(14) == 2484);
  assert(wigleFrequencyMHz(36) == 5180);
  assert(wigleFrequencyMHz(177) == 5885);
  assert(wigleFrequencyMHz(0) == 0);
  assert(wigleFrequencyMHz(200) == 0);

  uint8_t core_mac[6] = {0};
  assert(parseCoreMac("AA:BB:CC:DD:EE:02", core_mac));
  assert(core_mac[0] == 0xAA && core_mac[5] == 0x02);
  assert(parseCoreMac("aa-bb-cc-dd-ee-02", core_mac));
  assert(!parseCoreMac("AA:BB:CC:DD:EE", core_mac));
  assert(!parseCoreMac("AA:BB:CC:DD:EE:GG", core_mac));
  assert(!parseCoreMac("00:00:00:00:00:00", core_mac));
  assert(!parseCoreMac("FF:FF:FF:FF:FF:FF", core_mac));
  assert(!parseCoreMac("AB:BB:CC:DD:EE:02", core_mac));
  char normalized_core[18];
  formatCoreMac(core_mac, normalized_core);
  assert(strcmp(normalized_core, "AA:BB:CC:DD:EE:02") == 0);
  uint8_t other_core[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x04};
  assert(coreMacMatches(core_mac, core_mac));
  assert(!coreMacMatches(core_mac, other_core));
  return 0;
}
