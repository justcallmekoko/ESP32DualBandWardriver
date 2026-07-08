#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "configs.h"
#include "utils.h"

#define STD_MSG  1
#define WARN_MSG 2
#define GUD_MSG  3

// ============================================================
// Web log ring buffer — captures the last LOG_RING_SIZE lines
// so they can be served via the /log web endpoint.
// ============================================================
#define LOG_RING_SIZE   100
#define LOG_LINE_MAX    120  // max chars per line including prefix

class Logger {
public:
  static void log(uint8_t type, String msg);

  // Ring buffer — public so serveConfigPage() can read it directly
  static String ring[LOG_RING_SIZE];
  static int    ring_head;
  static int    ring_count;

  // SD debug log — enabled/disabled from web UI
  // Call enableSDLog(true) after SD is mounted and setting is loaded.
  static bool   sd_log_enabled;
  static void   enableSDLog(bool enable);
};

#endif
