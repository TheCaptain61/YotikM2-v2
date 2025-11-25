// Profiles.cpp
#include "Profiles.h"

void applyCropProfile(uint8_t id, SystemSettings &s) {
  s.cropProfile = id;

  switch (id) {
    case 1: // 🍅 Помидоры
      s.comfortTempMin       = 20.0f;
      s.comfortTempMax       = 28.0f;
      s.comfortHumMin        = 40.0f;
      s.comfortHumMax        = 65.0f;
      s.soilMoistureSetpoint = 55.0f;
      s.soilMoistureHysteresis = 5.0f;
      s.lightLuxMin          = 6000.0f;
      s.lightMode            = 2;
      s.climateMode          = 1;
      s.wateringStartHour    = 6;
      s.wateringEndHour      = 22;
      s.lightCutoffHour      = 20;
      break;

    case 2: // 🥒 Огурцы
      s.comfortTempMin       = 22.0f;
      s.comfortTempMax       = 30.0f;
      s.comfortHumMin        = 50.0f;
      s.comfortHumMax        = 80.0f;
      s.soilMoistureSetpoint = 65.0f;
      s.soilMoistureHysteresis = 5.0f;
      s.lightLuxMin          = 5000.0f;
      s.lightMode            = 2;
      s.climateMode          = 2;
      s.wateringStartHour    = 5;
      s.wateringEndHour      = 23;
      s.lightCutoffHour      = 21;
      break;

    case 3: // 🌿 Зелень
      s.comfortTempMin       = 18.0f;
      s.comfortTempMax       = 24.0f;
      s.comfortHumMin        = 45.0f;
      s.comfortHumMax        = 70.0f;
      s.soilMoistureSetpoint = 55.0f;
      s.soilMoistureHysteresis = 4.0f;
      s.lightLuxMin          = 4000.0f;
      s.lightMode            = 1;
      s.climateMode          = 0;
      s.wateringStartHour    = 6;
      s.wateringEndHour      = 21;
      s.lightCutoffHour      = 19;
      break;

    case 4: // 🌺 Гибискус
      s.comfortTempMin       = 20.0f;
      s.comfortTempMax       = 26.0f;
      s.comfortHumMin        = 45.0f;
      s.comfortHumMax        = 65.0f;
      s.soilMoistureSetpoint = 60.0f;
      s.soilMoistureHysteresis = 4.0f;
      s.lightLuxMin          = 200.0f;
      s.lightMode            = 1;
      s.climateMode          = 0;
      s.wateringStartHour    = 7;
      s.wateringEndHour      = 21;
      s.lightCutoffHour      = 21;
      break;

    default: // 0 = custom
      break;
  }
}
