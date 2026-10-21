#pragma once

#include <stddef.h>
#include <stdint.h>

inline int coreMacHexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

inline bool parseCoreMac(const char* text, uint8_t out[6]) {
  if (!text || !out) return false;

  uint8_t parsed[6] = {0};
  size_t byte_index = 0;
  int high_nibble = -1;

  for (const char* cursor = text; *cursor; ++cursor) {
    if (*cursor == ':' || *cursor == '-') continue;

    const int nibble = coreMacHexValue(*cursor);
    if (nibble < 0 || byte_index >= 6) return false;

    if (high_nibble < 0) {
      high_nibble = nibble;
    }
    else {
      parsed[byte_index++] = (uint8_t)((high_nibble << 4) | nibble);
      high_nibble = -1;
    }
  }

  if (byte_index != 6 || high_nibble >= 0) return false;

  bool all_zero = true;
  bool all_ff = true;
  for (size_t i = 0; i < 6; ++i) {
    all_zero = all_zero && parsed[i] == 0;
    all_ff = all_ff && parsed[i] == 0xFF;
  }
  if (all_zero || all_ff || (parsed[0] & 0x01)) return false;

  for (size_t i = 0; i < 6; ++i) out[i] = parsed[i];
  return true;
}

inline bool coreMacMatches(const uint8_t expected[6], const uint8_t actual[6]) {
  if (!expected || !actual) return false;
  for (size_t i = 0; i < 6; ++i)
    if (expected[i] != actual[i]) return false;
  return true;
}

inline void formatCoreMac(const uint8_t mac[6], char out[18]) {
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  size_t cursor = 0;
  for (size_t i = 0; i < 6; ++i) {
    out[cursor++] = HEX_DIGITS[(mac[i] >> 4) & 0x0F];
    out[cursor++] = HEX_DIGITS[mac[i] & 0x0F];
    if (i != 5) out[cursor++] = ':';
  }
  out[cursor] = '\0';
}
