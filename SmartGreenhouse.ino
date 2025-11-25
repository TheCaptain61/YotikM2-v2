#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "Config.h"
#include "Devices.h"
#include "DisplayManager.h"
#include "Automation.h"
#include "Profiles.h"
#include "EEPROMManager.h"
#include "WebInterface.h"
#include "TelegramBotHandler.h"

extern Automation         g_automation;
extern WebInterface       g_web;
extern EEPROMManager      g_eeprom;
extern DisplayManager     g_display;
extern TelegramBotHandler g_telegram;

unsigned long lastSensorRead = 0;

void printGreenhouseTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(F("[TIME] Время не установлено (нет NTP)"));
    return;
  }

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

  Serial.print(F("[TIME][MSK] "));
  Serial.println(buf);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n=== Smart Greenhouse / TM1637 & Telegram UI ==="));

  g_eeprom.begin();
  g_eeprom.loadSettings(g_settings);

  applyCropProfile(g_settings.cropProfile, g_settings);

  g_devices.begin();
  g_display.begin();
  g_automation.begin();
  g_web.begin();

  // Московский часовой пояс
  configTzTime("MSK-3", "pool.ntp.org", "time.nist.gov");

  // 🟢 Инициализируем Telegram только если мы в STA и подключены к роутеру
  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {
    g_telegram.begin();
  } else {
    Serial.println(F("Telegram: пропускаем инициализацию (нет STA-соединения)"));
  }

  lastSensorRead = millis();
}

void loop() {
  static unsigned long lastTimePrint = 0;

  unsigned long now = millis();

  if ((long)(now - lastSensorRead) >= (long)Constants::SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = now;
    g_devices.readSensors();

    Serial.println(F("\n--- Текущее состояние ---"));
    Serial.printf("Воздух:  T=%.1f°C H=%.1f%% P=%.1f hPa\n",
                  g_sensorData.airTemperature,
                  g_sensorData.airHumidity,
                  g_sensorData.airPressure);
    Serial.printf("Почва:   W=%.1f%%\n",
                  g_sensorData.soilMoisture);
    Serial.printf("Свет:    L=%.1f lux\n", g_sensorData.lightLevelLux);
  }

  if (now - lastTimePrint > 60000UL) {
    lastTimePrint = now;
    printGreenhouseTime();
  }

  g_devices.loop();
  g_automation.loop();
  g_display.update();
  g_web.loop();
  g_telegram.loop();

  delay(5);
}