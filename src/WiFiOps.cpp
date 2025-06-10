#include "WiFiOps.h"

// Add compiler.c.elf.extra_flags=-Wl,-zmuldefs to platform.txt
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){
  if (arg == 31337)
    return 1;
  else
    return 0;
}