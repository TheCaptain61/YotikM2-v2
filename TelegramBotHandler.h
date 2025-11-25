// TelegramBotHandler.h
#ifndef TELEGRAM_BOT_HANDLER_H
#define TELEGRAM_BOT_HANDLER_H

#include "Config.h"
#include "Profiles.h"
#include "Devices.h"
#include "Automation.h"
#include "Profiles.h"
#include "EEPROMManager.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

class TelegramBotHandler {
public:
  void begin();
  void loop();

  // Можно вызывать из других модулей, если захочешь отправлять сообщения
  void notify(const String &msg);

private:
  WiFiClientSecure    client;
  UniversalTelegramBot* bot = nullptr;

  bool   notificationsEnabled = true;
  String primaryChatId;  // Куда слать уведомления (последний чат или CHAT_ID из Config.h)

  unsigned long lastPollMs        = 0;
  unsigned long lastAlertCheckMs  = 0;
  unsigned long lastDryAlertMs    = 0;
  unsigned long lastHotAlertMs    = 0;
  unsigned long lastColdAlertMs   = 0;
  unsigned long lastSensorAlertMs = 0;

  static constexpr unsigned long POLL_INTERVAL_MS  = 3000;             // опрос входящих сообщений
  static constexpr unsigned long ALERT_CHECK_MS    = 10000;            // проверка тревог
  static constexpr unsigned long ALERT_INTERVAL_MS = 15UL*60UL*1000UL; // не спамить чаще 15 минут

  void handleNewMessages(int n);
  void handleCommand(const String &chat_id, const String &text);
  void sendStatus(const String &chat_id);
  void sendHelp(const String &chat_id);
  void sendMainMenu(const String &chat_id);
  void checkAndSendAlerts();
};

extern TelegramBotHandler g_telegram;

#endif // TELEGRAM_BOT_HANDLER_H