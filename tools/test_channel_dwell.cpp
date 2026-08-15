#include <assert.h>
#include <stdint.h>

#include "ChannelDwell.h"

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
  return 0;
}
