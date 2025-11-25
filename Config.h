// Config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===== Версия настроек для EEPROM =====
// ПОВЫШЕНА до 2, чтобы корректно подхватились новые поля (оффсеты)
#define SETTINGS_VERSION 2
#define EEPROM_SIZE      2048

// ===== Пины (подправь под своё железо при необходимости) =====
namespace Pins {
  // Реле
  constexpr uint8_t RELAY_PUMP   = 17;
  constexpr uint8_t RELAY_FAN    = 16;
  constexpr uint8_t RELAY_LIGHT  = 5;

  // Серво двери
  constexpr uint8_t SERVO_DOOR   = 19;

  // Датчики почвы (MGS-TH50)
  constexpr uint8_t SOIL_MOISTURE = 34; // влажность
  constexpr uint8_t SOIL_TEMP     = 35; // температура (аналог второго канала)

  // Концевик двери (если есть)
  constexpr uint8_t DOOR_SENSOR   = 12;

  // TM1637
  constexpr uint8_t TM1637_CLK    = 15;
  constexpr uint8_t TM1637_DIO    = 14;

  // I2C (ESP32)
  constexpr uint8_t I2C_SDA       = 21;
  constexpr uint8_t I2C_SCL       = 22;

  // LED-матрица (WS2812B 8x8)
  constexpr uint8_t LED_DATA      = 18;  // если свет не работает — поменяй этот пин
}

// Логика реле
namespace RelayLogic {
  constexpr bool ACTIVE_HIGH = true; // если HIGH = включено
}

// ===== Константы =====
namespace Constants {
  constexpr uint16_t SOIL_ADC_MAX      = 4095;
  constexpr float    SOIL_TEMP_VREF    = 3.3f;
  constexpr uint8_t  NUM_LEDS          = 64;

  constexpr uint8_t  SERVO_CLOSED_ANGLE = 0;
  constexpr uint8_t  SERVO_OPEN_ANGLE   = 120;

  constexpr unsigned long SERVO_MOVE_DELAY_MS   = 800;
  constexpr unsigned long SENSOR_READ_INTERVAL_MS = 5000;
  constexpr unsigned long AUTOMATION_INTERVAL_MS  = 5000;

  constexpr unsigned long PUMP_COOLDOWN_MS     = 5UL * 60UL * 1000UL;
  constexpr unsigned long PUMP_PULSE_MS       = 1400;
  constexpr unsigned long PUMP_DAILY_LIMIT_MS = 15UL * 60UL * 1000UL;
  constexpr unsigned long DAILY_RESET_MS      = 24UL * 60UL * 60UL * 1000UL;
}

// ===== Настройки системы =====

struct SystemSettings {
  uint8_t version = SETTINGS_VERSION;

  // Wi-Fi
  char wifiSSID[32]     = "Greenhouse";
  char wifiPassword[32] = "12345678";

  // Климат
  float comfortTempMin = 22.0f;
  float comfortTempMax = 28.0f;
  float comfortHumMin  = 40.0f;
  float comfortHumMax  = 70.0f;

  // Влажность почвы
  float soilMoistureSetpoint   = 55.0f;
  float soilMoistureHysteresis = 5.0f;

  // Освещённость (BH1750)
  float lightLuxMin = 5000.0f; // ниже – включаем фитолампу

  // Режим фитолампы (LED-матрица)
  // 0 = белый, 1 = фитолампа (вегетация), 2 = фитолампа (цветение)
  uint8_t lightMode = 1;

  // Профиль культуры:
  // 0 = пользовательский, 1 = помидоры, 2 = огурцы, 3 = зелень
  uint8_t cropProfile = 0;

  // Разрешения
  bool automationEnabled = true;
  bool allowNightLight   = false;

  // Калибровка MGS-TH50 (ADC)
  int soilDryADC = 2800; // сухая почва
  int soilWetADC = 1200; // очень влажно / вода

  // --- Калибровочные смещения ---
  // Температура воздуха (°C), добавляется к показаниям BME280
  float airTempOffset  = 0.0f;
  // Влажность воздуха (%), добавляется к показаниям BME280
  float airHumOffset   = 0.0f;
  // Температура почвы (°C), добавляется к вычисленной температуре со 2-го канала
  float soilTempOffset = 0.0f;
};

struct SensorData {
  // Воздух
  float airTemperature = NAN;
  float airHumidity    = NAN;
  float airPressure    = NAN;

  // Почва
  float soilTemperature = NAN;
  float soilMoisture    = NAN; // 0–100%

  // Свет
  float lightLevelLux   = NAN;

  // Состояния устройств
  bool pumpOn   = false;
  bool fanOn    = false;
  bool lightOn  = false;
  bool doorOpen = false;

  // Диагностика
  bool sensorsHealthy = true;
  String lastError    = "";
};

// Конфигурация устройств (автоопределение)
struct DeviceConfig {
  // I2C
  bool    hasBME280  = false;
  bool    hasBH1750  = false;
  uint8_t bmeAddr    = 0x00;
  uint8_t bhAddr     = 0x00;
  bool    bmeHealthy = false;
  bool    bhHealthy  = false;

  // Почва
  bool hasSoilSensor = false;
  bool soilHealthy   = false;

  // LED и серво
  bool hasLEDMatrix = false;
  bool hasServo     = false;
};

// Телеграм
namespace TelegramConfig {
  // ВСТАВЬ сюда свои реальные данные
  constexpr const char* BOT_TOKEN = "8376762831:AAH_qShjmjXSOTNi0mPbaBGdfyhZfWHv1qE";
  constexpr const char* CHAT_ID   = "1058732426";
}

// Глобалы
extern SystemSettings g_settings;
extern SensorData     g_sensorData;
extern DeviceConfig   g_deviceConfig;

#endif // CONFIG_H