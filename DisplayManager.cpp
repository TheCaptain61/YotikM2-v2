// DisplayManager.cpp
#include "DisplayManager.h"
#include <math.h>

DisplayManager g_display;

void DisplayManager::begin() {
  display.setBrightness(0x0f);
  display.clear();
}

void DisplayManager::showValue(char prefix, int value) {
  // Формат: [префикс][десятки][единицы][пусто]
  uint8_t segs[4] = {0,0,0,0};

  // Буква
  switch (prefix) {
    case 'T': segs[0] = display.encodeDigit(0); segs[0] = 0b01111000; break; // T
    case 'H': segs[0] = 0b01110110; break; // H
    case 'S': segs[0] = 0b01101101; break; // S
    case 't': segs[0] = 0b01111000; break; // t
    default:  segs[0] = display.encodeDigit(0); break;
  }

  if (value < -9) value = -9;
  if (value > 99) value = 99;

  int absVal = abs(value);
  int tens   = absVal / 10;
  int ones   = absVal % 10;

  segs[1] = display.encodeDigit(tens);
  segs[2] = display.encodeDigit(ones);

  display.setSegments(segs);
}

void DisplayManager::update() {
  unsigned long now = millis();
  if (now - lastUpdate < 2000) return;
  lastUpdate = now;

  mode = (mode + 1) % 4;

  float val = NAN;
  char prefix = 'T';

  switch (mode) {
    case 0: // T воздух
      val = g_sensorData.airTemperature;
      prefix = 'T';
      break;
    case 1: // H воздух
      val = g_sensorData.airHumidity;
      prefix = 'H';
      break;
    case 2: // Soil moisture
      val = g_sensorData.soilMoisture;
      prefix = 'S';
      break;
    case 3: // T почва
      val = g_sensorData.soilTemperature;
      prefix = 't';
      break;
  }

  if (isnan(val)) {
    display.showNumberDec(0, true);
    return;
  }

  int n = (int)round(val);
  showValue(prefix, n);
}