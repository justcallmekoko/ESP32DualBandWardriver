#include "logger.h"

void Logger::log(uint8_t type, String msg) {
  String prefix = "";

  if (type == WARN_MSG) {
    prefix = "[!] ";
  }
  else if (type == GUD_MSG) {
    prefix = "[+] ";
  }
  else if (type == STD_MSG) {
    prefix = "[-] ";
  }
  else {
    prefix = "[-] ";
  }

  Serial.println(prefix + msg);
}
