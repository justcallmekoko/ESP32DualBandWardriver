#include <assert.h>
#include <stdint.h>

#include "ChannelDwell.h"

int main() {
  assert(calculateSoloDwellMs(80, 0, 10) == 80);
  assert(calculateSoloDwellMs(80, 5, 10) == 160);
  assert(calculateSoloDwellMs(80, 10, 10) == 240);
  assert(calculateSoloDwellMs(80, 10, 0) == 80);
  assert(calculateSoloDwellMs(80, 10, 10, 1) == 80);
  assert(calculateSoloDwellMs(0, 10, 10) == 0);
  assert(calculateSoloDwellMs(40000, 10, 10) == UINT16_MAX);
  assert(calculatePopularityBarHeight(0, 10, 48) == 0);
  assert(calculatePopularityBarHeight(5, 10, 48) == 24);
  assert(calculatePopularityBarHeight(1, 100, 48) == 1);
  assert(calculatePopularityBarHeight(10, 10, 48) == 48);
  assert(calculatePopularityBarHeight(20, 10, 48) == 48);
  assert(calculatePopularityBarHeight(5, 0, 48) == 0);
  assert(calculatePopularityBarHeight(5, 10, 0) == 0);
  return 0;
}
