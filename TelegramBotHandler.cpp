// TelegramBotHandler.cpp
#include "TelegramBotHandler.h"

extern Automation     g_automation;
extern Devices        g_devices;
extern EEPROMManager  g_eeprom;

TelegramBotHandler g_telegram;

void TelegramBotHandler::begin() {
  if (strlen(TelegramConfig::BOT_TOKEN) < 5) {
    Serial.println(F("⚠️ Telegram: BOT_TOKEN не задан"));
    return;
  }

  client.setInsecure(); // если хочешь жёсткий TLS — можно настроить fingerprint
  bot = new UniversalTelegramBot(TelegramConfig::BOT_TOKEN, client);

  if (strlen(TelegramConfig::CHAT_ID) > 0) {
    primaryChatId = TelegramConfig::CHAT_ID;
  }

  Serial.println(F("🤖 Telegram бот инициализирован"));
}

String TelegramBotHandler::mainKeyboardJson() {
  // reply-клавиатура:
  //  [ "📊 Статус", "💧 Полив" ]
  //  [ "⚙ Авто ВКЛ", "⏸ Авто ВЫКЛ" ]
  //  [ "🌡 Профили", "🔔 Увед. ВКЛ/ВЫКЛ" ]
  String k = "["
             "[\"📊 Статус\",\"💧 Полив\"],"
             "[\"⚙ Авто ВКЛ\",\"⏸ Авто ВЫКЛ\"],"
             "[\"🌡 Профили\",\"🔔 Увед. ВКЛ/ВЫКЛ\"]"
             "]";
  return k;
}

void TelegramBotHandler::loop() {
  if (!bot) return;

  unsigned long now = millis();

  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    int n = bot->getUpdates(bot->last_message_received + 1);
    if (n > 0) {
      handleNewMessages(n);
    }
  }

  if (notificationsEnabled && now - lastSensorAlertMs >= ALERT_CHECK_MS) {
    checkAndSendAlerts();
  }
}

void TelegramBotHandler::handleNewMessages(int n) {
  for (int i = 0; i < n; ++i) {
    const telegramMessage &msg = bot->messages[i];
    String chat_id = msg.chat_id;
    String text    = msg.text;

    if (primaryChatId.length() == 0) {
      primaryChatId = chat_id;
    }

    handleCommand(chat_id, text);
  }
}

void TelegramBotHandler::handleCommand(const String &chat_id, const String &text) {
  String t = text;
  t.trim();

  if (t == "/start") {
    sendMainMenu(chat_id);
    return;
  }

  if (t == "/help") {
    sendHelp(chat_id);
    return;
  }

  if (t == "/status" || t == "📊 Статус") {
    sendStatus(chat_id);
    return;
  }

  if (t == "/auto_on" || t == "⚙ Авто ВКЛ") {
    g_settings.automationEnabled = true;
    g_eeprom.saveSettings(g_settings);
    bot->sendMessageWithReplyKeyboard(chat_id,
                                      "✅ Автоматика включена",
                                      "HTML",
                                      mainKeyboardJson(),
                                      true);
    return;
  }

  if (t == "/auto_off" || t == "⏸ Авто ВЫКЛ") {
    g_settings.automationEnabled = false;
    g_eeprom.saveSettings(g_settings);
    bot->sendMessageWithReplyKeyboard(chat_id,
                                      "⏸ Автоматика выключена",
                                      "HTML",
                                      mainKeyboardJson(),
                                      true);
    return;
  }

  if (t == "/water_now" || t == "💧 Полив") {
    g_devices.setPump(true, 1200);
    bot->sendMessageWithReplyKeyboard(chat_id,
                                      "💧 Запущен импульсный полив",
                                      "HTML",
                                      mainKeyboardJson(),
                                      true);
    return;
  }

  if (t == "/notify_on") {
    notificationsEnabled = true;
    bot->sendMessageWithReplyKeyboard(chat_id,
                                      "🔔 Уведомления включены",
                                      "HTML",
                                      mainKeyboardJson(),
                                      true);
    return;
  }

  if (t == "/notify_off") {
    notificationsEnabled = false;
    bot->sendMessageWithReplyKeyboard(chat_id,
                                      "🔕 Уведомления выключены",
                                      "HTML",
                                      mainKeyboardJson(),
                                      true);
    return;
  }

  if (t == "🔔 Увед. ВКЛ/ВЫКЛ") {
    notificationsEnabled = !notificationsEnabled;
    bot->sendMessageWithReplyKeyboard(chat_id,
                                      notificationsEnabled ? "🔔 Уведомления включены"
                                                           : "🔕 Уведомления выключены",
                                      "HTML",
                                      mainKeyboardJson(),
                                      true);
    return;
  }

  if (t == "🌡 Профили" || t == "/profiles") {
    sendProfileMenu(chat_id);
    return;
  }

  // Быстрая смена профиля кнопками
  if (t == "🍅 Помидоры") {
    applyCropProfile(1, g_settings);
    g_eeprom.saveSettings(g_settings);
    bot->sendMessageWithReplyKeyboard(chat_id,
        "✅ Профиль: помидоры", "HTML", mainKeyboardJson(), true);
    return;
  }
  if (t == "🥒 Огурцы") {
    applyCropProfile(2, g_settings);
    g_eeprom.saveSettings(g_settings);
    bot->sendMessageWithReplyKeyboard(chat_id,
        "✅ Профиль: огурцы", "HTML", mainKeyboardJson(), true);
    return;
  }
  if (t == "🌿 Зелень") {
    applyCropProfile(3, g_settings);
    g_eeprom.saveSettings(g_settings);
    bot->sendMessageWithReplyKeyboard(chat_id,
        "✅ Профиль: зелень", "HTML", mainKeyboardJson(), true);
    return;
  }
  if (t == "🌺 Гибискус") {
    applyCropProfile(4, g_settings);
    g_eeprom.saveSettings(g_settings);
    bot->sendMessageWithReplyKeyboard(chat_id,
        "✅ Профиль: гибискус", "HTML", mainKeyboardJson(), true);
    return;
  }

  // Команды типа /set_soil_target 60
  if (t.startsWith("/set_soil_target")) {
    int val = t.substring(String("/set_soil_target").length()).toInt();
    if (val >= 20 && val <= 90) {
      g_settings.soilMoistureSetpoint = (float)val;
      g_eeprom.saveSettings(g_settings);
      bot->sendMessageWithReplyKeyboard(chat_id,
        "✅ Целевая влажность почвы: " + String(val) + "%",
        "HTML",
        mainKeyboardJson(),
        true);
    } else {
      bot->sendMessage(chat_id,
        "⚠️ Значение должно быть от 20 до 90",
        "HTML");
    }
    return;
  }

  // Если ничего не узнали
  bot->sendMessageWithReplyKeyboard(chat_id,
    "Неизвестная команда. Нажми кнопку или /help",
    "HTML",
    mainKeyboardJson(),
    true);
}

void TelegramBotHandler::sendStatus(const String &chat_id) {
  String msg;
  msg.reserve(512);

  msg  = "🌱 <b>Состояние теплицы</b>\n\n";
  msg += "🌡 Воздух: ";
  msg += String(g_sensorData.airTemperature,1);
  msg += "°C, ";
  msg += String(g_sensorData.airHumidity,1);
  msg += "%\n";

  msg += "🌱 Почва: ";
  msg += String(g_sensorData.soilMoisture,1);
  msg += "%\n";

  msg += "💡 Свет: ";
  msg += String(g_sensorData.lightLevelLux,1);
  msg += " lux\n\n";

  msg += "⚙️ Автоматика: ";
  msg += (g_settings.automationEnabled ? "ВКЛ" : "ВЫКЛ");
  msg += "\n";

  msg += "🚿 Насос: ";
  msg += (g_sensorData.pumpOn ? "ВКЛ" : "ВЫКЛ");
  msg += "\n";

  msg += "🌀 Вентиляция: ";
  msg += (g_sensorData.fanOn ? "ВКЛ" : "ВЫКЛ");
  msg += "\n";

  msg += "🚪 Дверь: ";
  msg += (g_sensorData.doorOpen ? "ОТКРЫТА" : "ЗАКРЫТА");
  msg += "\n";

  msg += "🥗 Профиль: ";
  switch (g_settings.cropProfile) {
    case 1: msg += "помидоры"; break;
    case 2: msg += "огурцы";   break;
    case 3: msg += "зелень";   break;
    case 4: msg += "гибискус"; break;
    default: msg += "custom";  break;
  }

  bot->sendMessageWithReplyKeyboard(chat_id,
                                    msg,
                                    "HTML",
                                    mainKeyboardJson(),
                                    true);
}

void TelegramBotHandler::sendMainMenu(const String &chat_id) {
  String msg = "Привет! Это умная теплица ЙоТик M2.\n"
               "Нажимай кнопки или введи /help для списка команд.";
  bot->sendMessageWithReplyKeyboard(chat_id,
                                    msg,
                                    "HTML",
                                    mainKeyboardJson(),
                                    true);
}

void TelegramBotHandler::sendProfileMenu(const String &chat_id) {
  // Отдельная клавиатура для выбора профиля
  String k = "["
             "[\"🍅 Помидоры\",\"🥒 Огурцы\"],"
             "[\"🌿 Зелень\",\"🌺 Гибискус\"],"
             "[\"⬅ Назад\"]"
             "]";
  String msg = "Выбери профиль культуры:";
  bot->sendMessageWithReplyKeyboard(chat_id,
                                    msg,
                                    "HTML",
                                    k,
                                    true);
}

void TelegramBotHandler::sendHelp(const String &chat_id) {
  String msg;
  msg  = "🌱 <b>Умная теплица ЙоТик M2</b>\n\n";
  msg += "<b>Основные команды:</b>\n";
  msg += "📊 Статус — текущее состояние\n";
  msg += "💧 Полив — импульсный полив\n";
  msg += "⚙ Авто ВКЛ / ⏸ Авто ВЫКЛ — управление автоматикой\n";
  msg += "🌡 Профили — быстрый выбор культуры\n";
  msg += "🔔 Увед. ВКЛ/ВЫКЛ — уведомления\n\n";
  msg += "<b>Текстовые команды:</b>\n";
  msg += "<code>/status</code>, <code>/auto_on</code>, <code>/auto_off</code>\n";
  msg += "<code>/notify_on</code>, <code>/notify_off</code>\n";
  msg += "<code>/water_now</code>\n";
  msg += "<code>/set_soil_target 60</code> — целевая влажность почвы\n";
  msg += "<code>/profiles</code> — меню профилей\n";
  bot->sendMessageWithReplyKeyboard(chat_id,
                                    msg,
                                    "HTML",
                                    mainKeyboardJson(),
                                    true);
}

void TelegramBotHandler::checkAndSendAlerts() {
  unsigned long now = millis();

  bool sensorProblem = false;
  String sensorMsg;

  if (isnan(g_sensorData.airTemperature) || isnan(g_sensorData.airHumidity)) {
    sensorProblem = true;
    sensorMsg += "Датчик климата (BME280) не отвечает.\n";
  }
  if (isnan(g_sensorData.soilMoisture)) {
    sensorProblem = true;
    sensorMsg += "Датчик влажности почвы не отвечает.\n";
  }
  if (isnan(g_sensorData.lightLevelLux)) {
    sensorProblem = true;
    sensorMsg += "Датчик освещённости (BH1750) не отвечает.\n";
  }

  if (sensorProblem && (now - lastSensorAlertMs > ALERT_INTERVAL_MS)) {
    String msg = "⚙️ Проблемы с датчиками:\n" + sensorMsg;
    if (primaryChatId.length() > 0) {
      bot->sendMessage(primaryChatId, msg, "HTML");
    }
    lastSensorAlertMs = now;
  }
}

void TelegramBotHandler::notify(const String &msg) {
  if (!bot) return;
  if (primaryChatId.length() == 0) return;
  bot->sendMessage(primaryChatId, msg, "HTML");
}