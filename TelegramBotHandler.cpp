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

  client.setInsecure();
  bot = new UniversalTelegramBot(TelegramConfig::BOT_TOKEN, client);

  unsigned long now = millis();
  lastPollMs        = now;
  lastAlertCheckMs  = now;
  lastDryAlertMs    = 0;
  lastHotAlertMs    = 0;
  lastColdAlertMs   = 0;
  lastSensorAlertMs = 0;

  // Если CHAT_ID задан в Config.h — используем его как основной
  if (strlen(TelegramConfig::CHAT_ID) > 0) {
    primaryChatId = TelegramConfig::CHAT_ID;
  }

  Serial.println(F("🤖 Telegram-бот инициализирован"));
}

void TelegramBotHandler::loop() {
  if (!bot) return;
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();

  // Опрос входящих сообщений
  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    int n = bot->getUpdates(bot->last_message_received + 1);
    if (n) {
      handleNewMessages(n);
    }
  }

  // Проверка тревог / уведомлений
  if (now - lastAlertCheckMs >= ALERT_CHECK_MS) {
    lastAlertCheckMs = now;
    checkAndSendAlerts();
  }
}

void TelegramBotHandler::handleNewMessages(int n) {
  for (int i = 0; i < n; i++) {
    String chat_id = bot->messages[i].chat_id;
    String text    = bot->messages[i].text;

    // Сохраняем чат для уведомлений
    if (chat_id.length() > 0) {
      primaryChatId = chat_id;
    }

    handleCommand(chat_id, text);
  }
}

void TelegramBotHandler::sendMainMenu(const String &chat_id) {
  // Reply-клавиатура: строки с командами
  // Каждая строка — массив текстов кнопок; текст = команда, которая отправится
  String keyboard =
    "["
      "[\"/status\",\"/water_now\"],"
      "[\"/auto_on\",\"/auto_off\"],"
      "[\"/notify_on\",\"/notify_off\"],"
      "[\"/profile_tomatoes\",\"/profile_cucumbers\",\"/profile_greens\"]"
    "]";

  // true в конце — сделать клавиатуру "одноразовой" (можно убрать, если хочешь постоянную)
  bot->sendMessageWithReplyKeyboard(
    chat_id,
    "🌱 <b>Умная теплица ЙоТик М2</b>\nКнопки ниже отправляют команды.",
    "HTML",
    keyboard,
    true
  );
}

void TelegramBotHandler::sendHelp(const String &chat_id) {
  String msg;
  msg  = "🌱 <b>Умная теплица ЙоТик М2</b>\n\n";
  msg += "<b>Основные команды:</b>\n";
  msg += "<code>/status</code> - статус теплицы\n";
  msg += "<code>/auto_on</code>, <code>/auto_off</code> - включить/выключить автоматику\n";
  msg += "<code>/notify_on</code>, <code>/notify_off</code> - уведомления\n";
  msg += "<code>/water_now</code> - немедленный импульсный полив\n";
  msg += "<code>/set_soil_target 60</code> - целевая влажность почвы\n";
  msg += "<code>/set_profile tomatoes|cucumbers|greens|custom</code> - профиль культуры\n\n";
  msg += "<b>Ручное управление:</b>\n";
  msg += "<code>/pump_on</code> / <code>/pump_off</code>\n";
  msg += "<code>/fan_on</code> / <code>/fan_off</code>\n";
  msg += "<code>/light_on</code> / <code>/light_off</code>\n";
  msg += "<code>/door_open</code> / <code>/door_close</code>\n";

  bot->sendMessage(chat_id, msg, "HTML");
}

void TelegramBotHandler::handleCommand(const String &chat_id, const String &rawText) {
  String cmd = rawText;
  cmd.trim();

  // Если без "/", подменим help/status
  if (!cmd.startsWith("/")) {
    if (cmd.equalsIgnoreCase("help"))   cmd = "/help";
    if (cmd.equalsIgnoreCase("status")) cmd = "/status";
  }

  // === Меню / старт ===
  if (cmd == "/start") {
    sendMainMenu(chat_id);
    sendHelp(chat_id);
    return;
  }

  if (cmd == "/help") {
    sendHelp(chat_id);
    return;
  }

  // === Статус ===
  if (cmd == "/status") {
    sendStatus(chat_id);
    return;
  }

  // === Автоматизация ===
  if (cmd == "/auto_on") {
    g_settings.automationEnabled = true;
    g_eeprom.saveSettings(g_settings);
    bot->sendMessage(chat_id, "🤖 Автоматизация: <b>ВКЛ</b>", "HTML");
    return;
  }

  if (cmd == "/auto_off") {
    g_settings.automationEnabled = false;
    g_eeprom.saveSettings(g_settings);
    bot->sendMessage(chat_id, "🤖 Автоматизация: <b>ВЫКЛ</b>", "HTML");
    return;
  }

  // === Уведомления ===
  if (cmd == "/notify_on") {
    notificationsEnabled = true;
    bot->sendMessage(chat_id, "🔔 Уведомления: <b>ВКЛЮЧЕНЫ</b>", "HTML");
    return;
  }

  if (cmd == "/notify_off") {
    notificationsEnabled = false;
    bot->sendMessage(chat_id, "🔕 Уведомления: <b>ВЫКЛЮЧЕНЫ</b>", "HTML");
    return;
  }

  // === Полив ===
  if (cmd == "/water_now" || cmd == "/water") {
    g_devices.setPump(true, Constants::PUMP_PULSE_MS);
    bot->sendMessage(chat_id, "💧 Импульсный полив запущен", "HTML");
    return;
  }

  if (cmd == "/pump_on") {
    g_devices.setPump(true, Constants::PUMP_PULSE_MS);
    bot->sendMessage(chat_id, "💧 Насос ВКЛ (импульс)", "HTML");
    return;
  }

  if (cmd == "/pump_off") {
    g_devices.setPump(false, 0);
    bot->sendMessage(chat_id, "💧 Насос ВЫКЛ", "HTML");
    return;
  }

  // === Вентилятор / свет / дверь ===
  if (cmd == "/fan_on") {
    g_devices.setFan(true);
    bot->sendMessage(chat_id, "🌬️ Вентилятор ВКЛ", "HTML");
    return;
  }
  if (cmd == "/fan_off") {
    g_devices.setFan(false);
    bot->sendMessage(chat_id, "🌬️ Вентилятор ВЫКЛ", "HTML");
    return;
  }

  if (cmd == "/light_on") {
    g_devices.setLight(true);
    bot->sendMessage(chat_id, "💡 Свет ВКЛ", "HTML");
    return;
  }
  if (cmd == "/light_off") {
    g_devices.setLight(false);
    bot->sendMessage(chat_id, "💡 Свет ВЫКЛ", "HTML");
    return;
  }

  if (cmd == "/door_open") {
    g_devices.setDoorAngle(Constants::SERVO_OPEN_ANGLE);
    bot->sendMessage(chat_id, "🚪 Дверь ОТКРЫТА", "HTML");
    return;
  }
  if (cmd == "/door_close") {
    g_devices.setDoorAngle(Constants::SERVO_CLOSED_ANGLE);
    bot->sendMessage(chat_id, "🚪 Дверь ЗАКРЫТА", "HTML");
    return;
  }

  // === Настройки: целевая влажность почвы ===
  if (cmd.startsWith("/set_soil_target")) {
    int spaceIdx = cmd.indexOf(' ');
    if (spaceIdx > 0 && spaceIdx < (int)cmd.length()-1) {
      String valStr = cmd.substring(spaceIdx+1);
      float target = valStr.toFloat();
      if (target > 0 && target < 100) {
        g_settings.soilMoistureSetpoint = target;
        g_eeprom.saveSettings(g_settings);
        bot->sendMessage(chat_id, "🌱 Целевая влажность почвы: <b>" + String(target,1) + "%</b>", "HTML");
      } else {
        bot->sendMessage(chat_id, "❗ Укажи число от 1 до 99, например:\n<code>/set_soil_target 60</code>", "HTML");
      }
    } else {
      bot->sendMessage(chat_id, "Использование:\n<code>/set_soil_target 60</code>", "HTML");
    }
    return;
  }

  // === Настройки: профиль культуры ===
  if (cmd.startsWith("/set_profile") || cmd.startsWith("/profile_")) {
    String name;
    // кнопки вида /profile_tomatoes
    if (cmd.startsWith("/profile_")) {
      name = cmd.substring(String("/profile_").length());
    } else {
      int spaceIdx = cmd.indexOf(' ');
      if (spaceIdx > 0 && spaceIdx < (int)cmd.length()-1) {
        name = cmd.substring(spaceIdx+1);
      }
    }

    name.toLowerCase();

    uint8_t id = 0;
    if (name == "tomatoes" || name == "tomato" || name == "tomat" || name == "помидоры") id = 1;
    else if (name == "cucumbers" || name == "cucumber" || name == "огурцы") id = 2;
    else if (name == "greens" || name == "зелень") id = 3;
    else if (name == "custom" || name == "user") id = 0;

    applyCropProfile(id, g_settings);
    g_eeprom.saveSettings(g_settings);

    String caption;
    switch (id) {
      case 1: caption = "🍅 Профиль: помидоры"; break;
      case 2: caption = "🥒 Профиль: огурцы";   break;
      case 3: caption = "🌿 Профиль: зелень";   break;
      default: caption = "⚙️ Профиль: пользовательский"; break;
    }

    bot->sendMessage(chat_id, caption, "HTML");
    return;
  }

  // Если команда не распознана
  bot->sendMessage(chat_id, "Неизвестная команда. Напиши <code>/help</code>", "HTML");
}

void TelegramBotHandler::sendStatus(const String &chat_id) {
  float Ta = g_sensorData.airTemperature;
  float Ha = g_sensorData.airHumidity;
  float Ts = g_sensorData.soilTemperature;
  float Ms = g_sensorData.soilMoisture;
  float Lx = g_sensorData.lightLevelLux;

  float hoursSpan = 0.0f;
  float slope = g_automation.computeSoilDryingSlope(hoursSpan); // %/час

  float setpoint = g_settings.soilMoistureSetpoint;
  float hyster   = g_settings.soilMoistureHysteresis;
  float dryPoint = setpoint - hyster;

  String dryForecast = "—";

  if (!isnan(Ms) && slope < 0 && Ms > dryPoint) {
    float dM = dryPoint - Ms;      // отрицательное
    float hoursToDry = dM / slope; // slope < 0 → >0
    if (hoursToDry > 0 && hoursToDry < 72) {
      int h = int(hoursToDry);
      int m = int((hoursToDry - h) * 60);
      dryForecast = String(h) + " ч " + String(m) + " мин";
    }
  }

  String msg = "📊 <b>Статус теплицы</b>\n\n";

  msg += "🌡 <b>Воздух</b>: " + String(Ta,1) + "°C, " + String(Ha,1) + "%\n";
  msg += "🌱 <b>Почва</b>:  " + String(Ts,1) + "°C, " + String(Ms,1) + "%\n";
  msg += "💡 <b>Свет</b>:   " + String(Lx,1) + " lux\n\n";

  msg += "🚰 <b>Полив</b>\n";
  msg += "  Цель: " + String(setpoint,1) + "%\n";
  msg += "  Порог сухости: " + String(dryPoint,1) + "%\n";
  msg += "  Тренд влажности: ";
  if (hoursSpan < 0.15f) msg += "мало данных\n";
  else msg += String(slope,2) + " %/ч\n";
  msg += "  Прогноз до высыхания: " + dryForecast + "\n\n";

  msg += "🤖 <b>Режимы</b>\n";
  msg += "  Автоматизация: " + String(g_settings.automationEnabled ? "ВКЛ" : "выкл") + "\n";
  msg += "  Уведомления: "   + String(notificationsEnabled ? "ВКЛ" : "выкл") + "\n\n";

  msg += "🔌 <b>Устройства</b>\n";
  msg += "  Насос: "      + String(g_sensorData.pumpOn ? "ВКЛ" : "выкл") + "\n";
  msg += "  Вентилятор: " + String(g_sensorData.fanOn  ? "ВКЛ" : "выкл") + "\n";
  msg += "  Свет: "       + String(g_sensorData.lightOn ? "ВКЛ" : "выкл") + "\n";
  msg += "  Дверь: "      + String(g_sensorData.doorOpen ? "открыта" : "закрыта") + "\n";

  bot->sendMessage(chat_id, msg, "HTML");
}

void TelegramBotHandler::checkAndSendAlerts() {
  if (!notificationsEnabled) return;
  if (!bot) return;
  if (primaryChatId.length() == 0) return;

  unsigned long now = millis();

  float Ta = g_sensorData.airTemperature;
  float Ms = g_sensorData.soilMoisture;

  // 1) Слишком сухая почва
  float setpoint = g_settings.soilMoistureSetpoint;
  float hyster   = g_settings.soilMoistureHysteresis;
  float veryDry  = setpoint - 2*hyster; // сильно ниже цели

  if (!isnan(Ms) && Ms < veryDry && (now - lastDryAlertMs > ALERT_INTERVAL_MS)) {
    String msg = "⚠️ Почва слишком сухая: " + String(Ms,1) + "% (цель " + String(setpoint,1) + "%)\n"
                 "Полив уже старается догнать цель, но проверь систему.";
    bot->sendMessage(primaryChatId, msg, "HTML");
    lastDryAlertMs = now;
  }

  // 2) Слишком жарко
  if (!isnan(Ta) && Ta > g_settings.comfortTempMax + 5.0f && (now - lastHotAlertMs > ALERT_INTERVAL_MS)) {
    String msg = "🔥 В теплице очень жарко: " + String(Ta,1) + "°C\n"
                 "Проверь вентиляцию, растения могут перегреваться.";
    bot->sendMessage(primaryChatId, msg, "HTML");
    lastHotAlertMs = now;
  }

  // 3) Слишком холодно
  if (!isnan(Ta) && Ta < g_settings.comfortTempMin - 5.0f && (now - lastColdAlertMs > ALERT_INTERVAL_MS)) {
    String msg = "🥶 В теплице холодно: " + String(Ta,1) + "°C\n"
                 "Возможна остановка роста, подумай о подогреве.";
    bot->sendMessage(primaryChatId, msg, "HTML");
    lastColdAlertMs = now;
  }

  // 4) Проблемы с датчиками
  bool sensorProblem = false;
  String sensorMsg;

  if (!g_deviceConfig.bmeHealthy) {
    sensorProblem = true;
    sensorMsg += "BME280 (температура/влажность/давление) не отвечает.\n";
  }
  if (!g_deviceConfig.hasSoilSensor || !g_deviceConfig.soilHealthy) {
    sensorProblem = true;
    sensorMsg += "Датчик почвы не обнаружен или даёт некорректные данные.\n";
  }
  if (!g_deviceConfig.bhHealthy) {
    sensorProblem = true;
    sensorMsg += "BH1750 (освещённость) не отвечает.\n";
  }

  if (sensorProblem && (now - lastSensorAlertMs > ALERT_INTERVAL_MS)) {
    String msg = "⚙️ Проблемы с датчиками:\n" + sensorMsg;
    bot->sendMessage(primaryChatId, msg, "HTML");
    lastSensorAlertMs = now;
  }
}

void TelegramBotHandler::notify(const String &msg) {
  if (!bot) return;
  if (primaryChatId.length() == 0) return;
  bot->sendMessage(primaryChatId, msg, "HTML");
}