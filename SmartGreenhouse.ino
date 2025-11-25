// SmartGreenhouse.ino
/*
 * Умная теплица ЙоТик M2 — финальная версия:
 * ESP32 + BME280 + BH1750 + MGS-TH50 + реле + серво + LED-матрица (WS2812B) + TM1637
 * + Web интерфейс (мониторинг, управление, настройки, диагностика, Wi-Fi)
 * + EEPROM (настройки, калибровка)
 * + Профили культур
 * + Телеграм-бот
 */

#include <Arduino.h>
#include "Config.h"
#include "Devices.h"
#include "DisplayManager.h"
#include "Automation.h"
#include "Profiles.h"
#include "EEPROMManager.h"
#include "WebInterface.h"
#include "TelegramBotHandler.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

unsigned long lastSensorRead = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n=== Smart Greenhouse / Final Firmware ==="));

  g_eeprom.begin();
  g_eeprom.loadSettings(g_settings);

  g_devices.begin();
  g_display.begin();
  g_automation.begin();
  g_web.begin();
      // Московский часовой пояс
    configTzTime("MSK-3", "pool.ntp.org", "time.nist.gov");
  g_telegram.begin();

  lastSensorRead = millis();
}
void printGreenhouseTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(F("[TIME] Время не установлено (нет NTP)"));
    return;
  }

  char buf[32];
  // Формат: 2025-01-15 20:07:35
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

  Serial.print(F("[TIME] "));
  Serial.println(buf);
}
void loop() {
  static unsigned long lastTimePrint = 0;

  unsigned long now = millis();

  // --- Чтение датчиков ---
  if ((long)(now - lastSensorRead) >= (long)Constants::SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = now;
    g_devices.readSensors();

    Serial.println(F("\n--- Текущее состояние ---"));
    Serial.printf("Воздух:  T=%.1f°C H=%.1f%% P=%.1f hPa\n",
                  g_sensorData.airTemperature,
                  g_sensorData.airHumidity,
                  g_sensorData.airPressure);
    Serial.printf("Почва:   T=%.1f°C W=%.1f%%\n",
                  g_sensorData.soilTemperature,
                  g_sensorData.soilMoisture);
    Serial.printf("Свет:    L=%.1f lux\n", g_sensorData.lightLevelLux);
  }

  // --- Печать времени раз в 60 секунд ---
  if (now - lastTimePrint > 60000UL) {
    lastTimePrint = now;
    printGreenhouseTime();
  }

  g_devices.updatePump();
  g_automation.loop();
  g_display.update();
  g_web.loop();
  g_telegram.loop();

  delay(5);
}
