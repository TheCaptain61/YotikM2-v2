// Automation.h
#ifndef AUTOMATION_H
#define AUTOMATION_H

#include "Config.h"
#include "Devices.h"
#include "Profiles.h"

// Класс умной автоматики:
// - климат (дверь + вентилятор)
// - свет
// - продвинутый полив (с историей влажности и предиктивной логикой)
class Automation {
public:
  void begin();
  void loop();

  // Публично: нужно боту для отображения тренда влажности (%/час)
  // Возвращает наклон (%/час), <0 = сохнет, >0 = увлажняется
  // В hoursSpan возвращает охват истории в часах
  float computeSoilDryingSlope(float &hoursSpan);

private:
  // Период запуска автоматики (общий)
  unsigned long lastAutomationRun = 0;

  // ---- Климат (дверь + вентилятор) ----
  bool          doorCurrentlyOpen   = false;
  bool          fanCurrentlyOn      = false;
  unsigned long lastDoorChangeMs    = 0;
  unsigned long lastFanChangeMs     = 0;

  // Минимальное время работы/простоя, чтобы не дёргать реле и сервопривод
  static constexpr unsigned long FAN_MIN_ON_MS     = 30UL * 1000UL;
  static constexpr unsigned long FAN_MIN_OFF_MS    = 20UL * 1000UL;
  static constexpr unsigned long DOOR_MIN_OPEN_MS  = 60UL * 1000UL;
  static constexpr unsigned long DOOR_MIN_CLOSED_MS= 60UL * 1000UL;

  void handleClimate();
  void handleLighting();

  // ---- Полив ----
  // История влажности для «умного» анализа
  static const uint8_t  SOIL_HISTORY_SIZE = 10;
  float         soilMoistureHistory[SOIL_HISTORY_SIZE];
  unsigned long soilTimeHistory[SOIL_HISTORY_SIZE];
  uint8_t       soilHistoryCount = 0;
  uint8_t       soilHistoryIndex = 0;
  unsigned long lastSoilSampleMs = 0;

  // Когда в последний раз реально включали насос
  unsigned long lastPumpStartMs = 0;

  // Раз в сколько брать точку в историю (мс)
  static constexpr unsigned long SOIL_SAMPLE_INTERVAL_MS = 60UL * 1000UL; // 1 мин

  void handleWatering();
  void recordSoilHistory(float moisture, unsigned long nowMs);
};

extern Automation g_automation;

#endif // AUTOMATION_H