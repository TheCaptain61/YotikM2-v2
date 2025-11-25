// Devices.h
#ifndef DEVICES_H
#define DEVICES_H

#include "Config.h"
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_BME280.h>
#include <ESP32Servo.h>
#include <FastLED.h>

class Devices {
public:
  void begin();
  void readSensors();

  void loop() {
    updatePump();
    updateDoor();
  }

  // Управление железом
  void setPump(bool on, unsigned long pulseMs = 0);
  void setFan(bool on);
  void setLight(bool on);
  void setDoorAngle(uint8_t angle); // неблокирующее движение

private:
  // Датчики
  BH1750          lightMeter;
  Adafruit_BME280 bme;

  // Серво двери
  Servo  doorServo;
  bool   servoAttached         = false;
  uint8_t currentDoorAngle     = Constants::SERVO_CLOSED_ANGLE;
  uint8_t targetDoorAngle      = Constants::SERVO_CLOSED_ANGLE;
  bool   doorMoving            = false;
  unsigned long doorMoveStartMs    = 0;
  unsigned long doorMoveDurationMs = Constants::SERVO_MOVE_DURATION_MS;

  // Насос
  bool          pumpAutoOff      = false;
  unsigned long pumpOffAtMillis  = 0;
  unsigned long pumpDailyResetAt = 0;
  unsigned long totalPumpMsToday = 0;
  unsigned long lastPumpStartMs  = 0;
  unsigned long lastPumpPulseMs  = 0;

  void initSensors();
  void readBME280();
  void readBH1750();
  void readSoil();

  void updatePump();
  void updateDoor();

  friend class Automation;
};

extern Devices g_devices;

#endif // DEVICES_H