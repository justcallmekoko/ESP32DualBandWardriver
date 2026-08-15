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
  return 0;
}
