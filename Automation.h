// Automation.h
#ifndef AUTOMATION_H
#define AUTOMATION_H

#include "Config.h"
#include "Devices.h"
#include "Profiles.h"

class Automation {
public:
  void begin();
  void loop();

private:
  unsigned long lastAutomationRun = 0;

  // История влажности почвы
  static constexpr uint8_t  SOIL_HISTORY_MAX             = 32;
  static constexpr unsigned long SOIL_SAMPLE_INTERVAL_MS = 60UL * 1000UL;

  float         soilMoistureHistory[SOIL_HISTORY_MAX];
  unsigned long soilTimeHistory[SOIL_HISTORY_MAX];
  uint8_t       soilHistoryCount  = 0;
  uint8_t       soilHistoryIndex  = 0;
  unsigned long lastSoilSampleMs  = 0;

  // Состояние климата
  bool          doorCurrentlyOpen = false;
  bool          fanCurrentlyOn    = false;
  unsigned long lastDoorChangeMs  = 0;
  unsigned long lastFanChangeMs   = 0;

  static constexpr unsigned long DOOR_MIN_OPEN_MS   = 60UL * 1000UL;
  static constexpr unsigned long DOOR_MIN_CLOSED_MS = 60UL * 1000UL;
  static constexpr unsigned long FAN_MIN_ON_MS      = 30UL * 1000UL;
  static constexpr unsigned long FAN_MIN_OFF_MS     = 30UL * 1000UL;

  void handleClimate();
  void handleLighting();
  void handleWatering();

  bool  getLocalHour(uint8_t &hourOut);
  bool  isNightTime();
  bool  isWithinWateringWindow();
  float computeSoilDryingSlope();
  void  recordSoilHistory(float moisture, unsigned long nowMs);
};

extern Automation g_automation;

#endif // AUTOMATION_H