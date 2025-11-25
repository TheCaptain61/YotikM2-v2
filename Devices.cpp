// Devices.cpp
#include "Devices.h"
#include "EEPROMManager.h"

SystemSettings g_settings;
SensorData     g_sensorData;
DeviceConfig   g_deviceConfig;
Devices        g_devices;

CRGB g_leds[Constants::NUM_LEDS];

static inline void relayWrite(uint8_t pin, bool on) {
  if (RelayLogic::ACTIVE_HIGH) {
    digitalWrite(pin, on ? HIGH : LOW);
  } else {
    digitalWrite(pin, on ? LOW : HIGH);
  }
}

void Devices::begin() {
  // Реле
  pinMode(Pins::RELAY_PUMP,  OUTPUT);
  pinMode(Pins::RELAY_FAN,   OUTPUT);
  pinMode(Pins::RELAY_LIGHT, OUTPUT);
  relayWrite(Pins::RELAY_PUMP,  false);
  relayWrite(Pins::RELAY_FAN,   false);
  relayWrite(Pins::RELAY_LIGHT, false);

  // Серво двери
  doorServo.attach(Pins::SERVO_DOOR);
  servoAttached          = true;
  currentDoorAngle       = Constants::SERVO_CLOSED_ANGLE;
  targetDoorAngle        = currentDoorAngle;
  doorMoving             = false;
  doorMoveStartMs        = millis();
  g_sensorData.doorOpen  = false;
  g_deviceConfig.hasServo = true;

  // LED-лента/матрица
  FastLED.addLeds<WS2812B, Pins::LED_PIN, GRB>(g_leds, Constants::NUM_LEDS);
  FastLED.clear();
  FastLED.show();
  g_deviceConfig.hasLEDMatrix = true;

  initSensors();

  unsigned long now   = millis();
  pumpDailyResetAt    = now + Constants::DAILY_RESET_MS;
  totalPumpMsToday    = 0;
  pumpAutoOff         = false;
  lastPumpStartMs     = 0;
  lastPumpPulseMs     = 0;
}

void Devices::initSensors() {
  Wire.begin();

  // BME280
  if (bme.begin(0x76) || bme.begin(0x77)) {
    g_deviceConfig.hasBME280  = true;
    g_deviceConfig.bmeHealthy = true;
  }

  // BH1750
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    g_deviceConfig.hasBH1750  = true;
    g_deviceConfig.bhHealthy  = true;
  }

  // Почва
  pinMode(Pins::SOIL_ADC_PIN, INPUT);
  g_deviceConfig.hasSoilSensor = true;
}

void Devices::readSensors() {
  readBME280();
  readBH1750();
  readSoil();
}

void Devices::readBME280() {
  if (!g_deviceConfig.hasBME280) return;

  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0f;

  if (!isnan(t)) g_sensorData.airTemperature = t + g_settings.airTempOffset;
  if (!isnan(h)) g_sensorData.airHumidity    = h + g_settings.airHumOffset;
  if (!isnan(p)) g_sensorData.airPressure    = p;
}

void Devices::readBH1750() {
  if (!g_deviceConfig.hasBH1750) return;

  float lux = lightMeter.readLightLevel();
  if (!isnan(lux) && lux >= 0 && lux <= 65535) {
    g_sensorData.lightLevelLux = lux;
  }
}

void Devices::readSoil() {
  if (!g_deviceConfig.hasSoilSensor) return;

  uint16_t raw = analogRead(Pins::SOIL_ADC_PIN);
  raw          = constrain(raw, Constants::SOIL_ADC_MIN, Constants::SOIL_ADC_MAX);

  float moisture = 100.0f * (Constants::SOIL_ADC_MAX - raw) /
                   (Constants::SOIL_ADC_MAX - Constants::SOIL_ADC_MIN);

  g_sensorData.soilMoisture = moisture + g_settings.soilMoistOffset;
  // Температуру почвы можно добавить отдельным датчиком
}

// ===== Насос =====
void Devices::setPump(bool on, unsigned long pulseMs) {
  unsigned long now = millis();

  // Сброс дневного счётчика
  if ((long)(now - pumpDailyResetAt) >= 0) {
    pumpDailyResetAt = now + Constants::DAILY_RESET_MS;
    totalPumpMsToday = 0;
  }

  if (on) {
    if (totalPumpMsToday >= Constants::PUMP_DAILY_LIMIT_MS) {
      Serial.println(F("🚫 Лимит насоса на сегодня исчерпан"));
      g_sensorData.pumpOn = false;
      relayWrite(Pins::RELAY_PUMP, false);
      pumpAutoOff = false;
      return;
    }

    relayWrite(Pins::RELAY_PUMP, true);
    g_sensorData.pumpOn   = true;
    lastPumpStartMs       = now;
    lastPumpPulseMs       = pulseMs;
    pumpAutoOff           = (pulseMs > 0);
    pumpOffAtMillis       = now + pulseMs;
  } else {
    if (g_sensorData.pumpOn && lastPumpStartMs != 0) {
      unsigned long runMs = now - lastPumpStartMs;
      totalPumpMsToday   += runMs;
    }
    relayWrite(Pins::RELAY_PUMP, false);
    g_sensorData.pumpOn = false;
    pumpAutoOff         = false;
  }
}

void Devices::updatePump() {
  if (!pumpAutoOff)       return;
  if (!g_sensorData.pumpOn) return;

  unsigned long now = millis();
  if ((long)(now - pumpOffAtMillis) >= 0) {
    relayWrite(Pins::RELAY_PUMP, false);
    g_sensorData.pumpOn = false;
    pumpAutoOff         = false;

    if (lastPumpStartMs != 0) {
      unsigned long runMs = now - lastPumpStartMs;
      totalPumpMsToday   += runMs;
    }

    Serial.printf("💧 Насос авто-выкл, сегодня: %lus\n",
                  totalPumpMsToday / 1000UL);
  }
}

// ===== Вентилятор =====
void Devices::setFan(bool on) {
  relayWrite(Pins::RELAY_FAN, on);
  g_sensorData.fanOn = on;
}

// ===== Свет =====
void Devices::setLight(bool on) {
    relayWrite(Pins::RELAY_LIGHT, on);
    g_sensorData.lightOn = on;

    if (g_deviceConfig.hasLEDMatrix) {
        if (on) {
            FastLED.setBrightness(180);

            //
            // Фитолампа: глубокий красный + синий (приближение к 660nm + 450nm)
            //
            // Красный – основной для фотосинтеза
            // Синий – стимулирует рост зелёной массы
            //
            // Пропорция примерно:
            //   RED : BLUE = 4 : 1
            //
            // Для гибискуса — отличный спектр
            //

            const CRGB phytoColor = CRGB(255, 0, 60);  
            // Можно усилить синий:
            // const CRGB phytoColor = CRGB(255, 0, 80);
            // Можно сделать мягче:
            // const CRGB phytoColor = CRGB(255, 0, 40);

            for (int i = 0; i < Constants::NUM_LEDS; ++i) {
                g_leds[i] = phytoColor;
            }
        } else {
            FastLED.clear();
        }

        FastLED.show();
    }

    Serial.printf("💡 Свет %s\n", on ? "ВКЛ" : "ВЫКЛ");
}

// ===== Дверь (неблокирующая) =====
void Devices::setDoorAngle(uint8_t angle) {
  if (!servoAttached) {
    g_sensorData.doorOpen = false;
    return;
  }

  angle = constrain(angle, 0, 180);

  targetDoorAngle      = angle;
  doorMoveStartMs      = millis();
  doorMoveDurationMs   = Constants::SERVO_MOVE_DURATION_MS;
  doorMoving           = true;
}

void Devices::updateDoor() {
  if (!servoAttached) return;
  if (!doorMoving)    return;

  unsigned long now   = millis();
  unsigned long dt    = now - doorMoveStartMs;

  if (dt >= doorMoveDurationMs) {
    currentDoorAngle = targetDoorAngle;
    doorServo.write(currentDoorAngle);
    doorMoving       = false;
  } else {
    float progress   = (float)dt / (float)doorMoveDurationMs;
    progress         = constrain(progress, 0.0f, 1.0f);
    uint8_t angle    = (uint8_t)(currentDoorAngle +
                        (targetDoorAngle - currentDoorAngle) * progress);
    doorServo.write(angle);
    currentDoorAngle = angle;
  }

  g_sensorData.doorOpen =
    (currentDoorAngle >= (Constants::SERVO_OPEN_ANGLE - 5));
}