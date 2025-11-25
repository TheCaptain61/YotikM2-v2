// Automation.cpp
#include "Automation.h"

Automation g_automation;

void Automation::begin() {
  lastAutomationRun = millis();
  soilHistoryCount  = 0;
  soilHistoryIndex  = 0;
  lastSoilSampleMs  = millis();
  lastPumpStartMs   = 0;

  doorCurrentlyOpen = false;
  fanCurrentlyOn    = false;
  lastDoorChangeMs  = millis();
  lastFanChangeMs   = millis();
}

void Automation::loop() {
  if (!g_settings.automationEnabled) return;

  unsigned long now = millis();
  if ((long)(now - lastAutomationRun) < (long)Constants::AUTOMATION_INTERVAL_MS) return;
  lastAutomationRun = now;

  handleClimate();
  handleLighting();
  handleWatering();
}

// ================== К Л И М А Т ==================

void Automation::handleClimate() {
  float t = g_sensorData.airTemperature;
  float h = g_sensorData.airHumidity;

  if (isnan(t) || isnan(h)) return; // нечего регулировать

  unsigned long now = millis();

  // Пороги на основе комфортных значений из настроек
  float hotThreshold      = g_settings.comfortTempMax + 1.0f; // чуть выше комфортной
  float veryHotThreshold  = g_settings.comfortTempMax + 4.0f; // сильно жарко
  float humidThreshold    = g_settings.comfortHumMax  + 3.0f; // повышенная влажность
  float backTempThreshold = g_settings.comfortTempMax - 1.0f; // чтобы была гистерезис
  float backHumThreshold  = g_settings.comfortHumMax  - 5.0f;

  bool wantStrongVent = (t > veryHotThreshold && h > humidThreshold);
  bool wantMildVent   = (!wantStrongVent) && (t > hotThreshold || (t > g_settings.comfortTempMax && h > g_settings.comfortHumMax));
  bool wantNoVent     = (t < backTempThreshold && h < backHumThreshold);

  // ---- Сильное проветривание: дверь ОТКРЫТА + вентилятор ВКЛ ----
  if (wantStrongVent) {
    // Дверь открыть (если ещё не открыта и выдержали минимум)
    if (!doorCurrentlyOpen && (now - lastDoorChangeMs) > DOOR_MIN_CLOSED_MS) {
      g_devices.setDoorAngle(Constants::SERVO_OPEN_ANGLE);
      doorCurrentlyOpen = true;
      lastDoorChangeMs  = now;
    }
    // Вентилятор включить (если ещё не включен и выдержали минимум простоя)
    if (!fanCurrentlyOn && (now - lastFanChangeMs) > FAN_MIN_OFF_MS) {
      g_devices.setFan(true);
      fanCurrentlyOn   = true;
      lastFanChangeMs  = now;
    }
    return; // сильный режим приоритетный
  }

  // ---- Умеренное проветривание: дверь ЗАКРЫТА, вентилятор ВКЛ ----
  if (wantMildVent) {
    // Дверь закрыть (если открыта и минимум открытого времени прошёл)
    if (doorCurrentlyOpen && (now - lastDoorChangeMs) > DOOR_MIN_OPEN_MS) {
      g_devices.setDoorAngle(Constants::SERVO_CLOSED_ANGLE);
      doorCurrentlyOpen = false;
      lastDoorChangeMs  = now;
    }
    // Вентилятор включить
    if (!fanCurrentlyOn && (now - lastFanChangeMs) > FAN_MIN_OFF_MS) {
      g_devices.setFan(true);
      fanCurrentlyOn   = true;
      lastFanChangeMs  = now;
    }
    return;
  }

  // ---- Климат близок к норме: можно всё выключать, но не сразу ----
  if (wantNoVent) {
    // Вентилятор выключаем только если отработал минимум
    if (fanCurrentlyOn && (now - lastFanChangeMs) > FAN_MIN_ON_MS) {
      g_devices.setFan(false);
      fanCurrentlyOn   = false;
      lastFanChangeMs  = now;
    }
    // Дверь закрываем, если была открыта достаточно долго
    if (doorCurrentlyOpen && (now - lastDoorChangeMs) > DOOR_MIN_OPEN_MS) {
      g_devices.setDoorAngle(Constants::SERVO_CLOSED_ANGLE);
      doorCurrentlyOpen = false;
      lastDoorChangeMs  = now;
    }
  }
}

// ================== С В Е Т ==================
// Проверка, запрещено ли включение света по времени
// Возвращает true если сейчас 20:00–06:00
bool isNightTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        // Если время не получено – ведём себя безопасно: считаем что свет ВКЛ можно
        return false;
    }

    int hour = timeinfo.tm_hour;

    // Запрещённый период: 20:00 – 06:00
    return (hour >= 20 || hour < 6);
}

void Automation::handleLighting() {
    static float ambientLux = NAN;     // Окружающий свет без лампы
    float lux = g_sensorData.lightLevelLux;

    if (isnan(lux)) return;

    // Шаг 1 — если лампа выключена, обновляем окружающий свет
    if (!g_sensorData.lightOn) {
        ambientLux = lux;
    }

    // Шаг 2 — вычисляем освещённость для решения
    float decisionLux;
    if (!isnan(ambientLux) && g_sensorData.lightOn) {
        decisionLux = ambientLux;          // пока лампа горит — используем ambientLux
    } else {
        decisionLux = lux;
    }

    // Шаг 3 — критерий темноты (решение по внешнему свету)
    bool dark = decisionLux < g_settings.lightLuxMin;

    // Шаг 4 — день или ночь по lux (60+
    bool isDayLux = decisionLux > 60.0f;

    // Шаг 5 — пользователь разрешил свет ночью или нет
    bool allowNight = g_settings.allowNightLight;

    // Шаг 6 — запрет включать свет после 20:00
    bool nightTimeBlocked = isNightTime();   // true с 20:00 до 06:00

    // Логика: Включаем свет только если
    // - темно
    // - и (день по lux или разрешено ночью)
    // - и НЕ запрещено временем
    bool shouldLight = false;

    if (dark) {
        if ( (isDayLux || allowNight) && !nightTimeBlocked ) {
            shouldLight = true;
        }
    }

    g_devices.setLight(shouldLight);
}
// ================== П О Л И В (умный) ==================

void Automation::recordSoilHistory(float moisture, unsigned long nowMs) {
  // пишем точку истории раз в SOIL_SAMPLE_INTERVAL_MS
  if ((nowMs - lastSoilSampleMs) < SOIL_SAMPLE_INTERVAL_MS) return;
  lastSoilSampleMs = nowMs;

  soilMoistureHistory[soilHistoryIndex] = moisture;
  soilTimeHistory[soilHistoryIndex]     = nowMs;

  if (soilHistoryCount < SOIL_HISTORY_SIZE) {
    soilHistoryCount++;
  }
  soilHistoryIndex = (soilHistoryIndex + 1) % SOIL_HISTORY_SIZE;
}

// Возвращает наклон (%/час) по истории, <0 = сохнет, >0 = влажнеет
float Automation::computeSoilDryingSlope(float &hoursSpan) {
  hoursSpan = 0.0f;
  if (soilHistoryCount < 2) return 0.0f;

  uint8_t newestIdx = (soilHistoryIndex + SOIL_HISTORY_SIZE - 1) % SOIL_HISTORY_SIZE;
  uint8_t oldestIdx = (soilHistoryIndex + SOIL_HISTORY_SIZE - soilHistoryCount) % SOIL_HISTORY_SIZE;

  float     moistOld = soilMoistureHistory[oldestIdx];
  float     moistNew = soilMoistureHistory[newestIdx];
  unsigned long tOld = soilTimeHistory[oldestIdx];
  unsigned long tNew = soilTimeHistory[newestIdx];

  unsigned long dMs = tNew - tOld;
  if (dMs < 10000UL) { // меньше 10 секунд — нет смысла
    return 0.0f;
  }

  hoursSpan = dMs / (1000.0f * 3600.0f); // в часах
  if (hoursSpan <= 0.0f) return 0.0f;

  float dM = moistNew - moistOld; // % влажности
  float slope = dM / hoursSpan;   // % в час
  return slope;
}

void Automation::handleWatering() {
  float moist = g_sensorData.soilMoisture;
  if (isnan(moist)) return;

  unsigned long now = millis();

  // Записать точку в историю (используется для тренда)
  recordSoilHistory(moist, now);

  float setpoint   = g_settings.soilMoistureSetpoint;
  float hysteresis = g_settings.soilMoistureHysteresis;

  if (setpoint < 0)   setpoint = 0;
  if (setpoint > 100) setpoint = 100;
  if (hysteresis < 1) hysteresis = 1;

  float dryThreshold = setpoint - hysteresis;
  if (dryThreshold < 0) dryThreshold = 0;

  // Ограничения по насосу
  bool cooldownOk =
    (now - lastPumpStartMs) > Constants::PUMP_COOLDOWN_MS;

  bool dailyBudgetOk =
    (g_devices.getTotalPumpMsToday() + Constants::PUMP_PULSE_MS) <= Constants::PUMP_DAILY_LIMIT_MS;

  float soilT = g_sensorData.soilTemperature;
  bool soilTooCold = (!isnan(soilT) && soilT < 5.0f); // очень холодная почва — не льём

  // Если базовые ограничения не соблюдены — даже не думаем о поливе
  if (!cooldownOk || !dailyBudgetOk || soilTooCold) {
    return;
  }

  // Анализ тренда высыхания
  float hoursSpan = 0.0f;
  float slope = computeSoilDryingSlope(hoursSpan); // %/час

  bool haveTrend = (hoursSpan > 0.15f); // хотя бы ~9 минут истории
  bool isDry     = (moist <= dryThreshold);

  bool approachingDry = false;
  unsigned long predictedToDryMs = 0;

  if (haveTrend && slope < -0.5f && moist > dryThreshold) {
    // Считаем, через сколько часов упадём до dryThreshold
    // slope < 0, moist > dryThreshold
    float dM = dryThreshold - moist; // отрицательное значение
    float hoursToThresh = dM / slope; // получится >0 если всё ок
    if (hoursToThresh > 0.0f && hoursToThresh < 6.0f) { // в течение 6 часов
      approachingDry    = true;
      predictedToDryMs  = (unsigned long)(hoursToThresh * 3600.0f * 1000.0f);
    }
  }

  // Если тренд положительный (увлажняется) и причём сильно — вероятно, недавно поливали или датчик под водой
  bool moistureRisingFast = (haveTrend && slope > 3.0f); // +3%/час и больше

  bool shouldWater = false;

  if (isDry) {
    // Сухо здесь и сейчас, но не поливаем, если влажность уже активно растёт
    if (!moistureRisingFast) {
      shouldWater = true;
      Serial.println(F("💧 Полив: почва уже сухая по порогу"));
    }
  } else if (approachingDry) {
    // Ещё не сухо, но высохнет в ближайшие часы — можем полить заранее
    if (!moistureRisingFast) {
      shouldWater = true;
      Serial.printf("💧 Полив: предиктивно, через ~%lu мин станет сухо\n", predictedToDryMs / 60000UL);
    }
  }

  if (!shouldWater) return;

  // Уточнение по ночи: если очень темно и не хотим поливать ночью, можно добавить условие здесь.
  // Сейчас — поливаем в любое время суток.

  // Запускаем насос на импульс
  g_devices.setPump(true, Constants::PUMP_PULSE_MS);
  lastPumpStartMs = now;

  Serial.printf("💧 Импульс полива: влажность=%.1f%%, цель=%.1f%% (сухой порог=%.1f%%), тренд=%.2f %%/ч\n",
                moist, setpoint, dryThreshold, slope);
}