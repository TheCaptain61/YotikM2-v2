// TelegramBotHandler.h
#ifndef TELEGRAM_BOT_HANDLER_H
#define TELEGRAM_BOT_HANDLER_H

#include "Config.h"
#include "Profiles.h"
#include "Devices.h"
#include "Automation.h"
#include "EEPROMManager.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

class TelegramBotHandler {
public:
  void begin();
  void loop();

  void notify(const String &msg);

private:
  WiFiClientSecure      client;
  UniversalTelegramBot* bot = nullptr;

  String primaryChatId;
  bool   notificationsEnabled = true;

  unsigned long lastPollMs        = 0;
  unsigned long lastSensorAlertMs = 0;

  static constexpr unsigned long POLL_INTERVAL_MS  = 3000;
  static constexpr unsigned long ALERT_CHECK_MS    = 10000;
  static constexpr unsigned long ALERT_INTERVAL_MS = 15UL*60UL*1000UL;

  void handleNewMessages(int n);
  void handleCommand(const String &chat_id, const String &text);
  void sendStatus(const String &chat_id);
  void sendHelp(const String &chat_id);
  void sendMainMenu(const String &chat_id);
  void sendProfileMenu(const String &chat_id);
  void checkAndSendAlerts();

  String mainKeyboardJson();
};

extern TelegramBotHandler g_telegram;

#endif // TELEGRAM_BOT_HANDLER_H