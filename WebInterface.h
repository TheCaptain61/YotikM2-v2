// WebInterface.h
#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include "Config.h"
#include "Devices.h"
#include "Automation.h"
#include "EEPROMManager.h"
#include "Profiles.h"
#include <WiFi.h>
#include <WebServer.h>

class WebInterface {
public:
  void begin();
  void loop();

private:
  WebServer server{80};

  void setupWiFi();

  // Страницы
  void handleRoot();
  void handleDiagnosticsPage();
  void handleWifiPage();

  // API
  void handleSensors();
  void handleSettingsGet();
  void handleSettingsPost();
  void handleControl();
  void handleCalibrate();
  void handleDiagnosticsApi();
  void handleWifiScan();
  void handleWifiSet();

  String buildSensorsJson();
  String buildSettingsJson();
  String buildDiagnosticsJson();
};

extern WebInterface g_web;

#endif // WEB_INTERFACE_H