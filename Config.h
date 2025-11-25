// Config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===== Версия настроек для EEPROM =====
#define SETTINGS_VERSION 3
#define EEPROM_SIZE      2048

// ===== Пины =====
namespace Pins {
  // Реле
  constexpr uint8_t RELAY_PUMP   = 17;
  constexpr uint8_t RELAY_FAN    = 16;
  constexpr uint8_t RELAY_LIGHT  = 4;

  // Серво двери
  constexpr uint8_t SERVO_DOOR   = 19;

  // Датчик почвы (ADC)
  constexpr uint8_t SOIL_ADC_PIN = 34;

  // LED-лента/матрица
  constexpr uint8_t LED_PIN      = 18;

  // TM1637
  constexpr uint8_t TM1637_CLK   = 15;
  constexpr uint8_t TM1637_DIO   = 14;
}

// ===== Логика реле =====
namespace RelayLogic {
  constexpr bool ACTIVE_HIGH = true;
}

// ===== Константы системы =====
namespace Constants {
  constexpr uint16_t SOIL_ADC_MIN      = 800;
  constexpr uint16_t SOIL_ADC_MAX      = 4095;

  constexpr uint8_t  NUM_LEDS          = 64;

  constexpr uint8_t  SERVO_CLOSED_ANGLE = 0;
  constexpr uint8_t  SERVO_HALF_ANGLE   = 60;
  constexpr uint8_t  SERVO_OPEN_ANGLE   = 120;

  constexpr unsigned long SERVO_MOVE_DURATION_MS    = 800;
  constexpr unsigned long SENSOR_READ_INTERVAL_MS   = 5000;
  constexpr unsigned long AUTOMATION_INTERVAL_MS    = 5000;

  constexpr unsigned long PUMP_COOLDOWN_MS     = 5UL * 60UL * 1000UL;
  constexpr unsigned long PUMP_DAILY_LIMIT_MS  = 15UL * 60UL * 1000UL;
  constexpr unsigned long DAILY_RESET_MS       = 24UL * 60UL * 60UL * 1000UL;

  constexpr unsigned long DISPLAY_UPDATE_MS    = 5000;
}

// ===== Настройки системы (EEPROM) =====
struct SystemSettings {
  uint8_t version = SETTINGS_VERSION;

  // Wi-Fi
  char wifiSSID[32]     = "PATT";
  char wifiPassword[32] = "89396A1F61";

  // Климат (комфорт)
  float comfortTempMin = 22.0f;
  float comfortTempMax = 28.0f;
  float comfortHumMin  = 40.0f;
  float comfortHumMax  = 70.0f;

  // Режим климата: 0 = Eco, 1 = Normal, 2 = Aggressive
  uint8_t climateMode = 1;

  // Аварийные пороги
  float safetyTempMin = 4.0f;
  float safetyTempMax = 40.0f;

  // Влажность почвы
  float soilMoistureSetpoint   = 55.0f;
  float soilMoistureHysteresis = 5.0f;

  // Окно времени для полива
  uint8_t wateringStartHour = 6;
  uint8_t wateringEndHour   = 22;

  // Свет
  float   lightLuxMin     = 60.0f; // ниже — включаем свет
  uint8_t lightMode       = 1;
  uint8_t lightCutoffHour = 20;    // после этого часа свет запрещён

  // Профиль культуры
  uint8_t cropProfile = 0; // 0=custom

  // Автоматизация
  bool automationEnabled = true;
  bool allowNightLight   = false;

  // Калибровка
  float airTempOffset   = 0.0f;
  float airHumOffset    = 0.0f;
  float soilTempOffset  = 0.0f;
  float soilMoistOffset = 0.0f;
};

// ===== Текущие показания =====
struct SensorData {
  // Воздух
  float airTemperature = NAN;
  float airHumidity    = NAN;
  float airPressure    = NAN;

  // Почва
  float soilTemperature = NAN;
  float soilMoisture    = NAN;

  // Свет
  float lightLevelLux   = NAN;

  // Состояние устройств
  bool pumpOn   = false;
  bool fanOn    = false;
  bool lightOn  = false;
  bool doorOpen = false;

  // Диагностика
  bool   sensorsHealthy = true;
  String lastError      = "";
};

// ===== Конфигурация железа =====
struct DeviceConfig {
  bool    hasBME280  = false;
  bool    hasBH1750  = false;
  uint8_t bmeAddr    = 0x00;
  uint8_t bhAddr     = 0x00;
  bool    bmeHealthy = false;
  bool    bhHealthy  = false;

  bool hasSoilSensor = false;
  bool soilHealthy   = false;

  bool hasLEDMatrix  = false;
  bool hasServo      = false;

  bool hasTM1637     = false;
};

// Telegram (реальные токены)
namespace TelegramConfig {
  constexpr const char* BOT_TOKEN = "";
  constexpr const char* CHAT_ID   = "";
}

// Глобальные экземпляры (определены в Devices.cpp)
extern SystemSettings g_settings;
extern SensorData     g_sensorData;
extern DeviceConfig   g_deviceConfig;

#endif // CONFIG_H
