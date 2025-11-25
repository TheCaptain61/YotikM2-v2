// Devices.cpp
#include "Devices.h"
#include "EEPROMManager.h"

SystemSettings g_settings;
SensorData     g_sensorData;
DeviceConfig   g_deviceConfig;
Devices        g_devices;

static inline void relayWrite(uint8_t pin, bool on) {
  if (RelayLogic::ACTIVE_HIGH) {
    digitalWrite(pin, on ? HIGH : LOW);
  } else {
    digitalWrite(pin, on ? LOW : HIGH);
  }
}

void Devices::begin() {
  Serial.println(F("🔧 Инициализация устройств..."));
  initRelays();

  pinMode(Pins::SOIL_MOISTURE, INPUT);
  pinMode(Pins::SOIL_TEMP,     INPUT);
  pinMode(Pins::DOOR_SENSOR,   INPUT_PULLUP);

  initI2C();
  discoverI2CDevices();
  initBME280();
  initBH1750();
  initServo();
  initLEDMatrix();
  initSoilSensors();

  pumpDailyResetAt = millis() + Constants::DAILY_RESET_MS;

  Serial.println(F("✅ Устройства инициализированы"));
}

void Devices::initRelays() {
  pinMode(Pins::RELAY_PUMP,  OUTPUT);
  pinMode(Pins::RELAY_FAN,   OUTPUT);
  pinMode(Pins::RELAY_LIGHT, OUTPUT);

  relayWrite(Pins::RELAY_PUMP,  false);
  relayWrite(Pins::RELAY_FAN,   false);
  relayWrite(Pins::RELAY_LIGHT, false);
}

void Devices::initI2C() {
  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  delay(100);
}

void Devices::discoverI2CDevices() {
  g_deviceConfig.hasBME280  = false;
  g_deviceConfig.hasBH1750  = false;
  g_deviceConfig.bmeHealthy = false;
  g_deviceConfig.bhHealthy  = false;

  Serial.println(F("🔍 I2C сканирование..."));
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print(F("  I2C найден: 0x"));
      Serial.println(addr, HEX);

      if (addr == 0x76 || addr == 0x77) {
        g_deviceConfig.hasBME280 = true;
        g_deviceConfig.bmeAddr   = addr;
      }
      if (addr == 0x23 || addr == 0x5C) {
        g_deviceConfig.hasBH1750 = true;
        g_deviceConfig.bhAddr    = addr;
      }
    }
  }
}

void Devices::refreshI2CDevices() {
  discoverI2CDevices();
  initBME280();
  initBH1750();
}

void Devices::initBME280() {
  if (!g_deviceConfig.hasBME280) {
    Serial.println(F("⚠️ BME280 не найден на шине"));
    return;
  }

  if (bme.begin(g_deviceConfig.bmeAddr)) {
    g_deviceConfig.bmeHealthy = true;
    Serial.print(F("✅ BME280 инициализирован по адресу 0x"));
    Serial.println(g_deviceConfig.bmeAddr, HEX);
  } else {
    g_deviceConfig.bmeHealthy = false;
    Serial.println(F("❌ Не удалось инициализировать BME280"));
  }
}

void Devices::initBH1750() {
  if (!g_deviceConfig.hasBH1750) {
    Serial.println(F("⚠️ BH1750 не найден на шине"));
    return;
  }

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, g_deviceConfig.bhAddr)) {
    g_deviceConfig.bhHealthy = true;
    Serial.print(F("✅ BH1750 инициализирован по адресу 0x"));
    Serial.println(g_deviceConfig.bhAddr, HEX);
  } else {
    g_deviceConfig.bhHealthy = false;
    Serial.println(F("❌ Не удалось инициализировать BH1750"));
  }
}

void Devices::initServo() {
  doorServo.attach(Pins::SERVO_DOOR);
  servoAttached = true;
  g_deviceConfig.hasServo = true;
  setDoorAngle(Constants::SERVO_CLOSED_ANGLE);
  delay(Constants::SERVO_MOVE_DELAY_MS);
  Serial.println(F("✅ Серво двери инициализировано"));
}

void Devices::initLEDMatrix() {
  FastLED.addLeds<NEOPIXEL, Pins::LED_DATA>(leds, Constants::NUM_LEDS);
  FastLED.setBrightness(120);
  FastLED.clear(true);
  g_deviceConfig.hasLEDMatrix = true;
  Serial.println(F("✅ LED-матрица (WS2812B) инициализирована"));
}

void Devices::initSoilSensors() {
  int val = analogRead(Pins::SOIL_MOISTURE);
  if (val > 100 && val < (int)Constants::SOIL_ADC_MAX - 100) {
    g_deviceConfig.hasSoilSensor = true;
    g_deviceConfig.soilHealthy   = true;
    Serial.printf("✅ Датчик почвы обнаружен (ADC=%d)\n", val);
  } else {
    g_deviceConfig.hasSoilSensor = false;
    g_deviceConfig.soilHealthy   = false;
    Serial.printf("⚠️ Датчик почвы не обнаружен (ADC=%d)\n", val);
  }
}

// ===== Чтение датчиков =====

void Devices::readSensors() {
  if (g_deviceConfig.bmeHealthy)   readBME280();
  if (g_deviceConfig.bhHealthy)    readBH1750();
  readSoilSensors();
}

void Devices::readBME280() {
  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0f;

  // Применяем калибровочные смещения воздуха
  if (!isnan(t)) {
    g_sensorData.airTemperature = t + g_settings.airTempOffset;
  }
  if (!isnan(h)) {
    g_sensorData.airHumidity = h + g_settings.airHumOffset;
  }
  if (!isnan(p)) {
    g_sensorData.airPressure = p;
  }
}

void Devices::readBH1750() {
  float lux = lightMeter.readLightLevel();
  if (!isnan(lux) && lux >= 0 && lux <= 65535) {
    g_sensorData.lightLevelLux = lux;
  }
}

void Devices::readSoilSensors() {
  int rawMoist = analogRead(Pins::SOIL_MOISTURE);
  int rawTemp  = analogRead(Pins::SOIL_TEMP);

  // --- Влажность почвы ---
  if (rawMoist > 50 && rawMoist < (int)Constants::SOIL_ADC_MAX - 50) {
    g_deviceConfig.hasSoilSensor = true;
    g_deviceConfig.soilHealthy   = true;

    int dry = g_settings.soilDryADC;
    int wet = g_settings.soilWetADC;
    dry = constrain(dry, 0, (int)Constants::SOIL_ADC_MAX);
    wet = constrain(wet, 0, (int)Constants::SOIL_ADC_MAX);
    if (dry != wet) {
      long moist = map(rawMoist, dry, wet, 0, 100);
      g_sensorData.soilMoisture = constrain(moist, 0, 100);
    }
  } else {
    g_deviceConfig.soilHealthy = false;
  }

  // --- Температура почвы (канал SOIL_TEMP) ---
  if (rawTemp > 50 && rawTemp < (int)Constants::SOIL_ADC_MAX - 50) {
    float voltage = (rawTemp / (float)Constants::SOIL_ADC_MAX) * Constants::SOIL_TEMP_VREF;
    // Примерная формула как для TMP36 (можно потом подогнать)
    float tempC   = (voltage - 0.5f) * 100.0f;

    // Смещение для выравнивания с реальностью
    tempC += g_settings.soilTempOffset;

    g_sensorData.soilTemperature = tempC;
  }
}

// ===== Управление =====

void Devices::setPump(bool on, unsigned long pulseMs) {
  if (on && (totalPumpMsToday >= Constants::PUMP_DAILY_LIMIT_MS)) {
    Serial.println(F("⛔ Достигнут дневной лимит насоса, авто-полив заблокирован"));
    relayWrite(Pins::RELAY_PUMP, false);
    g_sensorData.pumpOn = false;
    pumpAutoOff = false;
    return;
  }

  relayWrite(Pins::RELAY_PUMP, on);
  g_sensorData.pumpOn = on;

  if (on && pulseMs > 0) {
    pumpAutoOff     = true;
    pumpOffAtMillis = millis() + pulseMs;
    Serial.printf("💧 Насос ВКЛ на %lu мс\n", pulseMs);
  } else {
    pumpAutoOff = false;
    Serial.printf("💧 Насос %s\n", on ? "ВКЛ" : "ВЫКЛ");
  }
}

void Devices::updatePump() {
  unsigned long now = millis();

  if (pumpAutoOff && (long)(now - pumpOffAtMillis) >= 0) {
    relayWrite(Pins::RELAY_PUMP, false);
    g_sensorData.pumpOn = false;
    pumpAutoOff = false;
    totalPumpMsToday += Constants::PUMP_PULSE_MS;
    Serial.println(F("💧 Насос авто-выключен по таймеру"));
  }

  if ((long)(now - pumpDailyResetAt) >= 0) {
    pumpDailyResetAt = now + Constants::DAILY_RESET_MS;
    totalPumpMsToday = 0;
    Serial.println(F("🕒 Сброшен дневной счётчик насоса"));
  }
}

void Devices::setFan(bool on) {
  relayWrite(Pins::RELAY_FAN, on);
  g_sensorData.fanOn = on;
  Serial.printf("🌬️ Вентилятор %s\n", on ? "ВКЛ" : "ВЫКЛ");
}

void Devices::setLight(bool on) {
  relayWrite(Pins::RELAY_LIGHT, on);
  g_sensorData.lightOn = on;

  if (g_deviceConfig.hasLEDMatrix) {
    if (on) {
      for (uint8_t i = 0; i < Constants::NUM_LEDS; i++) {
        switch (g_settings.lightMode) {
          case 1: // вегетация: больше синего
            leds[i].r = 60;
            leds[i].g = 0;
            leds[i].b = 180;
            break;
          case 2: // цветение: больше красного
            leds[i].r = 200;
            leds[i].g = 0;
            leds[i].b = 40;
            break;
          default: // белый
            leds[i] = CRGB::White;
            break;
        }
      }
    } else {
      fill_solid(leds, Constants::NUM_LEDS, CRGB::Black);
    }
    FastLED.show();
  }

  Serial.printf("💡 Свет %s\n", on ? "ВКЛ" : "ВЫКЛ");
}

void Devices::setDoorAngle(uint8_t angle) {
  if (!servoAttached) return;
  angle = constrain(angle, 0, 180);
  doorServo.write(angle);
  delay(Constants::SERVO_MOVE_DELAY_MS);
  g_sensorData.doorOpen = (angle >= (Constants::SERVO_OPEN_ANGLE - 5));
  Serial.printf("🚪 Дверь угол = %u°\n", angle);
}

bool Devices::isDaytimeByLux() const {
  float lux = g_sensorData.lightLevelLux;
  if (isnan(lux)) return true;     // если датчик глючит — считаем, что день
  return lux > 60.0f;              // всё, что выше ~60 lux, считаем днём
}


// ===== Калибровка датчика почвы (влажность) =====

void Devices::calibrateSoilSensor(bool inWater) {
  int raw = analogRead(Pins::SOIL_MOISTURE);
  if (raw <= 0 || raw >= (int)Constants::SOIL_ADC_MAX) {
    Serial.printf("⚠️ Калибровка почвы невозможна, ADC=%d\n", raw);
    return;
  }

  if (inWater) {
    g_settings.soilWetADC = raw;
    Serial.printf("💧 Калибровка MGS-TH50: ВОДА (ADC=%d)\n", raw);
  } else {
    g_settings.soilDryADC = raw;
    Serial.printf("💨 Калибровка MGS-TH50: ВОЗДУХ (ADC=%d)\n", raw);
  }

  g_eeprom.saveSettings(g_settings);
}