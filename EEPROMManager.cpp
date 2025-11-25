// EEPROMManager.cpp
#include "EEPROMManager.h"

EEPROMManager g_eeprom;

void EEPROMManager::begin() {
  EEPROM.begin(EEPROM_SIZE);
}

void EEPROMManager::loadSettings(SystemSettings &settings) {
  SystemSettings tmp;
  EEPROM.get(0, tmp);

  if (tmp.version != SETTINGS_VERSION) {
    Serial.println(F("⚠️ Версия настроек не совпадает, используем значения по умолчанию"));
    resetDefaults(settings);
    saveSettings(settings);
    return;
  }

  settings = tmp;
  Serial.println(F("✅ Настройки загружены из EEPROM"));
}

void EEPROMManager::saveSettings(const SystemSettings &settings) {
  EEPROM.put(0, settings);
  EEPROM.commit();
  Serial.println(F("💾 Настройки сохранены в EEPROM"));
}

void EEPROMManager::resetDefaults(SystemSettings &settings) {
  SystemSettings def;
  settings = def;
  Serial.println(F("🔄 Настройки сброшены к заводским"));
}