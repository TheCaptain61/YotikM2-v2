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

  // страницы
  void handleRoot();
  void handleNotFound();

  // API
  void handleSensors();
  void handleSettingsGet();
  void handleSettingsPost();
  void handleControl();
  void handleDiagnosticsApi();
  void handleWifiScan();
  void handleWifiSet();

  // JSON
  String buildSensorsJson();
  String buildSettingsJson();
  String buildDiagnosticsJson();

  // простая BASIC-авторизация
  bool ensureAuth();

  static constexpr const char* WEB_USER = "admin";
  static constexpr const char* WEB_PASS = "greenhouse";
};

extern WebInterface g_web;

#endif // WEB_INTERFACE_H