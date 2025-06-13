#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "configs.h"
#include "utils.h"

#define STD_MSG 1
#define WARN_MSG 2
#define GUD_MSG 3

class Logger {
public:
  static void log(uint8_t type, String msg);
};

#endif
