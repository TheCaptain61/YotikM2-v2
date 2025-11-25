// DisplayManager.cpp
#include "DisplayManager.h"
#include <math.h>

extern SensorData g_sensorData;

DisplayManager g_display;

void DisplayManager::begin() {
  display.setBrightness(0x0f, true); // максимум яркости, включён
  display.clear();
}

void DisplayManager::update() {
  unsigned long now = millis();
  if (now - lastUpdate < Constants::DISPLAY_UPDATE_MS) return;
  lastUpdate = now;

  mode = (mode + 1) % 4;

  switch (mode) {
    case 0: showAirTemp();     break;
    case 1: showAirHumidity(); break;
    case 2: showSoilMoisture();break;
    case 3: showLux();         break;
  }
}

// Формат: t XX   (t и XX)
// Мы используем кастомные сегменты для буквы 't' и пробел
static const uint8_t SEG_T = SEG_A | SEG_F | SEG_E | SEG_D; // что-то похожее на 't'

void DisplayManager::showAirTemp() {
  if (isnan(g_sensorData.airTemperature)) {
    display.clear();
    return;
  }
  int temp = (int)round(g_sensorData.airTemperature);
  temp = constrain(temp, -99, 99);

  uint8_t data[4];
  data[0] = SEG_T;
  if (temp < 0) {
    data[1] = SEG_G; // минус (по сути только горизонтальная палка) — можно заменить
    temp = -temp;
  } else {
    data[1] = 0;
  }
  int tens = temp / 10;
  int ones = temp % 10;
  data[2] = display.encodeDigit(tens);
  data[3] = display.encodeDigit(ones);

  display.setSegments(data);
}

void DisplayManager::showAirHumidity() {
  if (isnan(g_sensorData.airHumidity)) {
    display.clear();
    return;
  }
  int hum = (int)round(g_sensorData.airHumidity);
  hum = constrain(hum, 0, 99);
  // Формат: H XX
  uint8_t data[4];
  // H — как A + F + E + B + C + G
  uint8_t SEG_H = SEG_F | SEG_E | SEG_B | SEG_C | SEG_G;
  data[0] = SEG_H;
  data[1] = 0;
  int tens = hum / 10;
  int ones = hum % 10;
  data[2] = display.encodeDigit(tens);
  data[3] = display.encodeDigit(ones);
  display.setSegments(data);
}

void DisplayManager::showSoilMoisture() {
  if (isnan(g_sensorData.soilMoisture)) {
    display.clear();
    return;
  }
  int mos = (int)round(g_sensorData.soilMoisture);
  mos = constrain(mos, 0, 99);
  // Формат: S XX
  uint8_t data[4];
  // S — примерно как 5
  uint8_t SEG_S = SEG_A | SEG_F | SEG_G | SEG_C | SEG_D;
  data[0] = SEG_S;
  data[1] = 0;
  int tens = mos / 10;
  int ones = mos % 10;
  data[2] = display.encodeDigit(tens);
  data[3] = display.encodeDigit(ones);
  display.setSegments(data);
}

void DisplayManager::showLux() {
  if (isnan(g_sensorData.lightLevelLux)) {
    display.clear();
    return;
  }
  int l = (int)round(g_sensorData.lightLevelLux);
  if (l > 9999) l = 9999;
  if (l < 0) l = 0;
  // Формат: просто число (lux)
  display.showNumberDec(l, true);
}