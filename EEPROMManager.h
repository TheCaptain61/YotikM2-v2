// EEPROMManager.h
#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include "Config.h"
#include <EEPROM.h>

class EEPROMManager {
public:
  void begin();
  void loadSettings(SystemSettings &settings);
  void saveSettings(const SystemSettings &settings);
  void resetDefaults(SystemSettings &settings);
};

extern EEPROMManager g_eeprom;

#endif // EEPROM_MANAGER_H