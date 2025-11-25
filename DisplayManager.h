// DisplayManager.h
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Config.h"
#include <TM1637Display.h>

class DisplayManager {
public:
  void begin();
  void update();

private:
  TM1637Display display{Pins::TM1637_CLK, Pins::TM1637_DIO};
  unsigned long lastUpdate = 0;
  uint8_t       mode       = 0;

  void showValue(char prefix, int value);
};

extern DisplayManager g_display;

#endif // DISPLAY_MANAGER_H