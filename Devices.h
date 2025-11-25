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
  void updatePump();

  void setPump(bool on, unsigned long pulseMs = 0);
  void setFan(bool on);
  void setLight(bool on);
  void setDoorAngle(uint8_t angle);

  bool isDaytimeByLux() const;
  unsigned long getTotalPumpMsToday() const { return totalPumpMsToday; }

  void discoverI2CDevices();
  void refreshI2CDevices();

  bool bmeOK() const { return g_deviceConfig.bmeHealthy; }
  bool bhOK()  const { return g_deviceConfig.bhHealthy;  }

  void calibrateSoilSensor(bool inWater);

private:
  void initRelays();
  void initI2C();
  void initBME280();
  void initBH1750();
  void initServo();
  void initLEDMatrix();
  void initSoilSensors();

  void readBME280();
  void readBH1750();
  void readSoilSensors();

  BH1750          lightMeter;
  Adafruit_BME280 bme;
  Servo           doorServo;
  CRGB            leds[Constants::NUM_LEDS];

  bool servoAttached = false;

  bool          pumpAutoOff       = false;
  unsigned long pumpOffAtMillis   = 0;
  unsigned long pumpDailyResetAt  = 0;
  unsigned long totalPumpMsToday  = 0;
};

extern Devices g_devices;
extern SystemSettings g_settings;
extern SensorData     g_sensorData;
extern DeviceConfig   g_deviceConfig;

#endif // DEVICES_H