// Automation.cpp
#include "Automation.h"

Automation g_automation;

void Automation::begin() {
  lastAutomationRun = millis();
  soilHistoryCount  = 0;
  soilHistoryIndex  = 0;
  lastSoilSampleMs  = millis();

  doorCurrentlyOpen = false;
  fanCurrentlyOn    = false;
  lastDoorChangeMs  = millis();
  lastFanChangeMs   = millis();
}

void Automation::loop() {
  if (!g_settings.automationEnabled) return;

  unsigned long now = millis();
  if ((long)(now - lastAutomationRun) < (long)Constants::AUTOMATION_INTERVAL_MS) {
    return;
  }
  lastAutomationRun = now;

  if ((long)(now - lastSoilSampleMs) >= (long)SOIL_SAMPLE_INTERVAL_MS &&
      !isnan(g_sensorData.soilMoisture)) {
    recordSoilHistory(g_sensorData.soilMoisture, now);
    lastSoilSampleMs = now;
  }

  handleClimate();
  handleLighting();
  handleWatering();
}

// ===== Климат =====
void Automation::handleClimate() {
  float t = g_sensorData.airTemperature;
  float h = g_sensorData.airHumidity;

  if (isnan(t) || isnan(h)) return;

  unsigned long now = millis();

  // Аварийные пороги
  if (!isnan(g_settings.safetyTempMin) && t < g_settings.safetyTempMin) {
    g_devices.setFan(false);
    g_devices.setDoorAngle(Constants::SERVO_CLOSED_ANGLE);
    doorCurrentlyOpen = false;
    fanCurrentlyOn    = false;
    Serial.println(F("⚠️ Холоднее safetyTempMin — закрываемся"));
    return;
  }

  if (!isnan(g_settings.safetyTempMax) && t > g_settings.safetyTempMax) {
    g_devices.setFan(true);
    g_devices.setDoorAngle(Constants::SERVO_OPEN_ANGLE);
    doorCurrentlyOpen = true;
    fanCurrentlyOn    = true;
    Serial.println(F("⚠️ Жарче safetyTempMax — максимум проветривания"));
    return;
  }

  float baseMax = g_settings.comfortTempMax;
  float baseHum = g_settings.comfortHumMax;

  float hotDelta, veryHotDelta, backDelta;
  switch (g_settings.climateMode) {
    case 0: // Eco
      hotDelta     = 2.0f;
      veryHotDelta = 6.0f;
      backDelta    = 1.5f;
      break;
    case 2: // Aggressive
      hotDelta     = 0.5f;
      veryHotDelta = 2.0f;
      backDelta    = 0.5f;
      break;
    case 1:
    default:
      hotDelta     = 1.0f;
      veryHotDelta = 4.0f;
      backDelta    = 1.0f;
      break;
  }

  float hotThreshold      = baseMax + hotDelta;
  float veryHotThreshold  = baseMax + veryHotDelta;
  float backTempThreshold = baseMax - backDelta;
  float humidThreshold    = baseHum + 5.0f;
  float backHumThreshold  = baseHum - 5.0f;

  bool wantStrongVent = (t > veryHotThreshold && h > humidThreshold);
  bool wantMildVent   = (!wantStrongVent) &&
                        ((t > hotThreshold) || (h > baseHum));
  bool wantNoVent     = (t < backTempThreshold && h < backHumThreshold);

  if (wantStrongVent) {
    if (!doorCurrentlyOpen && (now - lastDoorChangeMs) > DOOR_MIN_CLOSED_MS) {
      g_devices.setDoorAngle(Constants::SERVO_OPEN_ANGLE);
      doorCurrentlyOpen = true;
      lastDoorChangeMs  = now;
    }
    if (!fanCurrentlyOn && (now - lastFanChangeMs) > FAN_MIN_OFF_MS) {
      g_devices.setFan(true);
      fanCurrentlyOn   = true;
      lastFanChangeMs  = now;
    }
    return;
  }

  if (wantMildVent) {
    if (!doorCurrentlyOpen && (now - lastDoorChangeMs) > DOOR_MIN_CLOSED_MS) {
      g_devices.setDoorAngle(Constants::SERVO_HALF_ANGLE);
      doorCurrentlyOpen = true;
      lastDoorChangeMs  = now;
    }
    if (!fanCurrentlyOn && (now - lastFanChangeMs) > FAN_MIN_OFF_MS) {
      g_devices.setFan(true);
      fanCurrentlyOn   = true;
      lastFanChangeMs  = now;
    }
    return;
  }

  if (wantNoVent) {
    if (doorCurrentlyOpen && (now - lastDoorChangeMs) > DOOR_MIN_OPEN_MS) {
      g_devices.setDoorAngle(Constants::SERVO_CLOSED_ANGLE);
      doorCurrentlyOpen = false;
      lastDoorChangeMs  = now;
    }
    if (fanCurrentlyOn && (now - lastFanChangeMs) > FAN_MIN_ON_MS) {
      g_devices.setFan(false);
      fanCurrentlyOn   = false;
      lastFanChangeMs  = now;
    }
  }
}

// ===== Время =====
bool Automation::getLocalHour(uint8_t &hourOut) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false;
  }
  hourOut = timeinfo.tm_hour;
  return true;
}

bool Automation::isNightTime() {
  uint8_t h;
  if (!getLocalHour(h)) {
    Serial.println(F("[TIME] Нет времени, считаем что ночь (блок подсветки)"));
    return true;
  }

  uint8_t cutoff = g_settings.lightCutoffHour;
  if (h >= cutoff || h < 6) return true;
  return false;
}

// ===== Свет =====
void Automation::handleLighting() {
  float lux = g_sensorData.lightLevelLux;
  bool haveLux = !isnan(lux);

  bool night = isNightTime();

  // Ночь → свет всегда выключен
  if (night) {
    if (g_sensorData.lightOn) {
      Serial.printf("[LIGHT] Ночь (cutoff %u), выключаем свет\n",
                    g_settings.lightCutoffHour);
      g_devices.setLight(false);
    }
    return;
  }

  // День, но нет показаний lux — оставляем как есть
  if (!haveLux) {
    Serial.println("[LIGHT] Нет данных lux, состояние света не трогаем");
    return;
  }

  const float LUX_ON_THRESHOLD = 60.0f;

  // Если свет уже горит — держим его до наступления ночи, lux игнорируем
  if (g_sensorData.lightOn) {
    Serial.printf("[LIGHT] День, свет уже включен (lux=%.1f) — оставляем включенным\n", lux);
    return;
  }

  // Свет выключен → решаем, включать ли
  if (lux < LUX_ON_THRESHOLD) {
    Serial.printf("[LIGHT] lux=%.1f < %.1f — включаем свет\n",
                  lux, LUX_ON_THRESHOLD);
    g_devices.setLight(true);
  } else {
    Serial.printf("[LIGHT] lux=%.1f >= %.1f — свет выключен, не включаем\n",
                  lux, LUX_ON_THRESHOLD);
  }
}


// ===== Окно полива =====
bool Automation::isWithinWateringWindow() {
  uint8_t h;
  if (!getLocalHour(h)) {
    return false;
  }

  uint8_t startH = g_settings.wateringStartHour;
  uint8_t endH   = g_settings.wateringEndHour;

  if (startH == endH) return true;
  if (startH < endH) {
    return (h >= startH && h < endH);
  } else {
    return (h >= startH || h < endH);
  }
}

// ===== Полив =====
void Automation::handleWatering() {
  float moist = g_sensorData.soilMoisture;
  if (isnan(moist)) return;

  if (!isWithinWateringWindow()) {
    return;
  }

  float setpoint = g_settings.soilMoistureSetpoint;
  float hyst     = g_settings.soilMoistureHysteresis;
  float lowTh    = setpoint - hyst;
  float highTh   = setpoint + hyst;

  float slope = computeSoilDryingSlope(); // %/час

  bool veryDryNow  = moist < (lowTh - 5.0f);
  bool dryNow      = moist < lowTh;
  bool wetEnough   = moist > highTh;

  if (veryDryNow) {
    Serial.printf("💧 Сильно сухо (%.1f%%), поливаем\n", moist);
    g_devices.setPump(true, 1400);
    return;
  }

  if (dryNow) {
    if (slope >= 0.1f) {
      Serial.printf("💧 Сухо (%.1f%%), тренд %.2f%%/ч — поливаем\n", moist, slope);
      g_devices.setPump(true, 1200);
    } else {
      Serial.printf("💧 Сухо (%.1f%%), тренд медленный — краткий полив\n", moist);
      g_devices.setPump(true, 800);
    }
    return;
  }

  if (wetEnough && g_sensorData.pumpOn) {
    Serial.printf("💧 Влаги достаточно (%.1f%%), выключаем насос\n", moist);
    g_devices.setPump(false);
  }
}

// ===== История влажности =====
void Automation::recordSoilHistory(float moisture, unsigned long nowMs) {
  if (soilHistoryCount < SOIL_HISTORY_MAX) {
    soilHistoryCount++;
  }

  soilMoistureHistory[soilHistoryIndex] = moisture;
  soilTimeHistory[soilHistoryIndex]     = nowMs;
  soilHistoryIndex = (soilHistoryIndex + 1) % SOIL_HISTORY_MAX;
}

float Automation::computeSoilDryingSlope() {
  if (soilHistoryCount < 4) return 0.0f;

  float n = (float)soilHistoryCount;

  double sumT = 0.0;
  double sumM = 0.0;
  double sumTT = 0.0;
  double sumTM = 0.0;

  uint8_t firstIdx = (soilHistoryIndex + SOIL_HISTORY_MAX - soilHistoryCount) %
                     SOIL_HISTORY_MAX;
  unsigned long t0 = soilTimeHistory[firstIdx];

  for (uint8_t i = 0; i < soilHistoryCount; ++i) {
    uint8_t idx = (firstIdx + i) % SOIL_HISTORY_MAX;
    unsigned long dtMs = soilTimeHistory[idx] - t0;
    double tHours = (double)dtMs / 3600000.0;
    double m      = soilMoistureHistory[idx];

    sumT  += tHours;
    sumM  += m;
    sumTT += tHours * tHours;
    sumTM += tHours * m;
  }

  double denom = (n * sumTT - sumT * sumT);
  if (fabs(denom) < 1e-6) return 0.0f;

  double a = (n * sumTM - sumT * sumM) / denom; // slope %/час

  if (a < -20.0 || a > 0.0) return 0.0f;

  return (float)(-a);
}