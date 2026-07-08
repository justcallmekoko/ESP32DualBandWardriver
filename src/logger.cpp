#include "logger.h"
#include <SD.h>

// Static member definitions
String Logger::ring[LOG_RING_SIZE];
int    Logger::ring_head      = 0;
int    Logger::ring_count     = 0;
bool   Logger::sd_log_enabled = false;

void Logger::enableSDLog(bool enable) {
  sd_log_enabled = enable;
  if (enable)
    Serial.println("[-] [DBG] SD debug logging enabled -> " + String(DEBUG_LOG_FILE));
  else
    Serial.println("[-] [DBG] SD debug logging disabled");
}

void Logger::log(uint8_t type, String msg) {
  String prefix = "";

  if (type == WARN_MSG)
    prefix = "[!] ";
  else if (type == GUD_MSG)
    prefix = "[+] ";
  else
    prefix = "[-] ";

  String line = prefix + msg;

  // Always write to Serial
  Serial.println(line);

  // Write to ring buffer — truncate if over max length
  if (line.length() > LOG_LINE_MAX)
    line = line.substring(0, LOG_LINE_MAX - 3) + "...";

  ring[ring_head] = line;
  ring_head = (ring_head + 1) % LOG_RING_SIZE;
  if (ring_count < LOG_RING_SIZE)
    ring_count++;

  // Write to SD debug log if enabled
  #ifdef HAS_SD
  if (sd_log_enabled) {
    File f = SD.open(DEBUG_LOG_FILE, FILE_APPEND);
    if (f) {
      f.println(line);
      f.close();
    }
  }
  #endif
}
