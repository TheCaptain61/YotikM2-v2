// WebInterface.cpp
#include <WiFi.h>
#include "WebInterface.h"
#include "Profiles.h"

WebInterface g_web;

void WebInterface::begin() {
  setupWiFi();

  server.on("/",             HTTP_GET,  [this]() { handleRoot(); });
  server.on("/diagnostics",  HTTP_GET,  [this]() { handleDiagnosticsPage(); });
  server.on("/wifi",         HTTP_GET,  [this]() { handleWifiPage(); });

  server.on("/api/sensors",     HTTP_GET,  [this]() { handleSensors(); });
  server.on("/api/settings",    HTTP_GET,  [this]() { handleSettingsGet(); });
  server.on("/api/settings",    HTTP_POST, [this]() { handleSettingsPost(); });
  server.on("/api/control",     HTTP_GET,  [this]() { handleControl(); });
  server.on("/api/calibrate",   HTTP_POST, [this]() { handleCalibrate(); });
  server.on("/api/diagnostics", HTTP_GET,  [this]() { handleDiagnosticsApi(); });
  server.on("/api/wifi_scan",   HTTP_GET,  [this]() { handleWifiScan(); });
  server.on("/api/wifi",        HTTP_POST, [this]() { handleWifiSet(); });

  server.begin();
  Serial.println(F("🌐 WebServer запущен на порту 80"));
}

void WebInterface::loop() {
  server.handleClient();
}

void WebInterface::setupWiFi() {
  Serial.println(F("📶 Запуск Wi-Fi..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_settings.wifiSSID, g_settings.wifiPassword);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("✅ Подключен к Wi-Fi STA, IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("⚠️ Не удалось подключиться к роутеру, поднимаем AP..."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("GreenhouseAP", "12345678");
    Serial.print(F("📡 AP IP: "));
    Serial.println(WiFi.softAPIP());
  }
}

// ===== HTML: главная =====

void WebInterface::handleRoot() {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>Smart Greenhouse</title>"
    "<style>"
    "body{font-family:sans-serif;background:#111;color:#eee;padding:1rem}"
    "a{color:#4caf50}"
    ".card{background:#222;padding:1rem;margin-bottom:1rem;border-radius:8px}"
    "button{padding:0.4rem 0.8rem;margin:0.2rem;border-radius:4px;border:none;cursor:pointer}"
    ".on{background:#4caf50;color:#fff}"
    ".off{background:#f44336;color:#fff}"
    "input,select{margin:0.2rem 0;padding:0.2rem 0.4rem;width:8rem}"
    "label{display:block;margin-top:0.3rem}"
    "</style></head><body>"
    "<h1>Smart Greenhouse</h1>"
    "<p><a href='/diagnostics'>Диагностика устройств</a> | "
    "<a href='/wifi'>Wi-Fi настройки</a></p>"

    "<div class='card' id='sensors'>Загрузка...</div>"

    "<div class='card'>"
    "<h3>Управление</h3>"
    "<div>Насос: "
      "<button onclick=\"ctrl('pump',1)\" class='on'>ВКЛ</button>"
      "<button onclick=\"ctrl('pump',0)\" class='off'>ВЫКЛ</button>"
    "</div>"
    "<div>Вентилятор: "
      "<button onclick=\"ctrl('fan',1)\" class='on'>ВКЛ</button>"
      "<button onclick=\"ctrl('fan',0)\" class='off'>ВЫКЛ</button>"
    "</div>"
    "<div>Свет: "
      "<button onclick=\"ctrl('light',1)\" class='on'>ВКЛ</button>"
      "<button onclick=\"ctrl('light',0)\" class='off'>ВЫКЛ</button>"
    "</div>"
    "<div>Дверь: "
      "<button onclick=\"ctrl('door',1)\" class='on'>ОТКРЫТЬ</button>"
      "<button onclick=\"ctrl('door',0)\" class='off'>ЗАКРЫТЬ</button>"
    "</div>"
    "</div>"

    "<div class='card'>"
    "<h3>Настройки</h3>"
    "<form id='settingsForm' onsubmit='saveSettings();return false;'>"
      "<label>Temp min: <input type='number' step='0.1' id='comfortTempMin'></label>"
      "<label>Temp max: <input type='number' step='0.1' id='comfortTempMax'></label>"
      "<label>Hum min: <input type='number' step='0.1' id='comfortHumMin'></label>"
      "<label>Hum max: <input type='number' step='0.1' id='comfortHumMax'></label>"
      "<label>Soil target %: <input type='number' step='0.1' id='soilMoistureSetpoint'></label>"
      "<label>Light min lux: <input type='number' step='1' id='lightLuxMin'></label>"
      "<label>Light mode: "
        "<select id='lightMode'>"
          "<option value='0'>0 - Белый</option>"
          "<option value='1'>1 - Вегетация</option>"
          "<option value='2'>2 - Цветение</option>"
        "</select>"
      "</label>"
      "<label>Профиль культуры: "
        "<select id='cropProfile'>"
          "<option value='0'>0 - Пользовательский</option>"
          "<option value='1'>1 - Помидоры</option>"
          "<option value='2'>2 - Огурцы</option>"
          "<option value='3'>3 - Зелень</option>"
        "</select>"
      "</label>"
      "<hr>"
      "<label>Offset T воздуха (°C): <input type='number' step='0.1' id='airTempOffset'></label>"
      "<label>Offset H воздуха (%): <input type='number' step='0.1' id='airHumOffset'></label>"
      "<label>Offset T почвы (°C): <input type='number' step='0.1' id='soilTempOffset'></label>"
      "<hr>"
      "<label><input type='checkbox' id='automationEnabled'> Автоматизация включена</label>"
      "<label><input type='checkbox' id='allowNightLight'> Светить ночью при темноте</label>"
      "<button type='submit' class='on'>Сохранить</button>"
    "</form>"
    "</div>"

    "<div class='card'>"
    "<h3>Калибровка MGS-TH50</h3>"
    "<p>Поставьте датчик в воздух/воду и нажмите кнопку.</p>"
    "<button class='on' onclick=\"calibrate('air')\">Калибровка сухо (воздух)</button>"
    "<button class='on' onclick=\"calibrate('water')\">Калибровка мокро (вода)</button>"
    "</div>"

    "<script>"
    "function num(v){var n=Number(v); if(isNaN(n)) return 0; return n;}"
    "function updateSensors(){"
      "fetch('/api/sensors').then(r=>r.json()).then(d=>{"
        "let h='';"
        "let Ta=num(d.airTemperature), Ha=num(d.airHumidity), Pa=num(d.airPressure);"
        "let Ts=num(d.soilTemperature), Ms=num(d.soilMoisture), Lx=num(d.lightLevelLux);"
        "h += '<b>Воздух</b>: '+Ta.toFixed(1)+'&deg;C, '+Ha.toFixed(1)+'%<br>'; "
        "h += 'Давление: '+Pa.toFixed(1)+' hPa<br>'; "
        "h += '<b>Почва</b>: '+Ts.toFixed(1)+'&deg;C, '+Ms.toFixed(1)+'%<br>'; "
        "h += '<b>Свет</b>: '+Lx.toFixed(1)+' lux<br>'; "
        "h += '<b>Устройства</b>: насос='+d.pumpOn+', вентилятор='+d.fanOn+', свет='+d.lightOn+', дверь='+d.doorOpen; "
        "document.getElementById('sensors').innerHTML=h;"
      "}).catch(e=>{document.getElementById('sensors').innerHTML='Ошибка чтения датчиков';});"
    "}"
    "function loadSettings(){"
      "fetch('/api/settings').then(r=>r.json()).then(s=>{"
        "document.getElementById('comfortTempMin').value = s.comfortTempMin;"
        "document.getElementById('comfortTempMax').value = s.comfortTempMax;"
        "document.getElementById('comfortHumMin').value  = s.comfortHumMin;"
        "document.getElementById('comfortHumMax').value  = s.comfortHumMax;"
        "document.getElementById('soilMoistureSetpoint').value = s.soilMoistureSetpoint;"
        "document.getElementById('lightLuxMin').value = s.lightLuxMin;"
        "document.getElementById('lightMode').value   = s.lightMode;"
        "document.getElementById('cropProfile').value = s.cropProfile;"
        "document.getElementById('airTempOffset').value  = s.airTempOffset;"
        "document.getElementById('airHumOffset').value   = s.airHumOffset;"
        "document.getElementById('soilTempOffset').value = s.soilTempOffset;"
        "document.getElementById('automationEnabled').checked = !!s.automationEnabled;"
        "document.getElementById('allowNightLight').checked   = !!s.allowNightLight;"
      "});"
    "}"
    "function ctrl(dev,on){fetch('/api/control?dev='+dev+'&on='+on).then(_=>updateSensors());}"
    "function saveSettings(){"
      "let data = new URLSearchParams();"
      "data.append('comfortTempMin', document.getElementById('comfortTempMin').value);"
      "data.append('comfortTempMax', document.getElementById('comfortTempMax').value);"
      "data.append('comfortHumMin',  document.getElementById('comfortHumMin').value);"
      "data.append('comfortHumMax',  document.getElementById('comfortHumMax').value);"
      "data.append('soilMoistureSetpoint', document.getElementById('soilMoistureSetpoint').value);"
      "data.append('lightLuxMin', document.getElementById('lightLuxMin').value);"
      "data.append('lightMode',   document.getElementById('lightMode').value);"
      "data.append('cropProfile', document.getElementById('cropProfile').value);"
      "data.append('airTempOffset',  document.getElementById('airTempOffset').value);"
      "data.append('airHumOffset',   document.getElementById('airHumOffset').value);"
      "data.append('soilTempOffset', document.getElementById('soilTempOffset').value);"
      "data.append('automationEnabled', document.getElementById('automationEnabled').checked ? '1':'0');"
      "data.append('allowNightLight',   document.getElementById('allowNightLight').checked ? '1':'0');"
      "fetch('/api/settings', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:data.toString()})"
        ".then(_=>{alert('Сохранено');});"
    "}"
    "function calibrate(type){"
      "let data = new URLSearchParams();"
      "data.append('target', type);"
      "fetch('/api/calibrate', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:data.toString()})"
        ".then(_=>{alert('Калибровка '+type+' выполнена');});"
    "}"
    "setInterval(updateSensors,3000);"
    "updateSensors();"
    "loadSettings();"
    "</script>"
    "</body></html>"
  );
  server.send(200, "text/html", html);
}

// ===== HTML: диагностика =====

void WebInterface::handleDiagnosticsPage() {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>Diagnostics</title>"
    "<style>"
    "body{font-family:sans-serif;background:#111;color:#eee;padding:1rem}"
    "a{color:#4caf50}"
    ".card{background:#222;padding:1rem;margin-bottom:1rem;border-radius:8px}"
    "</style></head><body>"
    "<h1>Диагностика устройств</h1>"
    "<p><a href='/'>← Назад</a></p>"
    "<div class='card' id='diag'>Загрузка...</div>"
    "<script>"
    "function updateDiag(){"
      "fetch('/api/diagnostics').then(r=>r.json()).then(d=>{"
        "let h='';"
        "h += '<b>BME280</b>: has='+d.bme.has+', healthy='+d.bme.healthy+', addr=0x'+d.bme.addr.toString(16)+'<br>'; "
        "h += '<b>BH1750</b>: has='+d.bh.has+', healthy='+d.bh.healthy+', addr=0x'+d.bh.addr.toString(16)+'<br>'; "
        "h += '<b>Почва</b>: has='+d.soil.has+', healthy='+d.soil.healthy+', raw='+d.soil.raw+'<br>'; "
        "h += '<b>LED-матрица</b>: has='+d.led.has+'<br>'; "
        "h += '<b>Серво</b>: has='+d.servo.has+'<br>'; "
        "document.getElementById('diag').innerHTML=h;"
      "}).catch(e=>{document.getElementById('diag').innerHTML='Ошибка диагностики';});"
    "}"
    "setInterval(updateDiag,5000);"
    "updateDiag();"
    "</script>"
    "</body></html>"
  );
  server.send(200, "text/html", html);
}

// ===== HTML: Wi-Fi =====

void WebInterface::handleWifiPage() {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>Wi-Fi настройки</title>"
    "<style>"
    "body{font-family:sans-serif;background:#111;color:#eee;padding:1rem}"
    "a{color:#4caf50}"
    ".card{background:#222;padding:1rem;margin-bottom:1rem;border-radius:8px}"
    "button{padding:0.4rem 0.8rem;margin:0.2rem;border-radius:4px;border:none;cursor:pointer}"
    "input{margin:0.2rem 0;padding:0.2rem 0.4rem;width:12rem}"
    "</style></head><body>"
    "<h1>Wi-Fi настройки</h1>"
    "<p><a href='/'>← Назад</a></p>"
    "<div class='card'>"
    "<h3>Доступные сети</h3>"
    "<button onclick='scan()'>Сканировать</button>"
    "<ul id='nets'></ul>"
    "</div>"
    "<div class='card'>"
    "<h3>Подключение</h3>"
    "<form onsubmit='setWifi();return false;'>"
      "SSID:<br><input id='ssid'><br>"
      "Пароль:<br><input id='pass' type='password'><br>"
      "<button type='submit'>Сохранить и перезагрузить</button>"
    "</form>"
    "</div>"
    "<script>"
    "function scan(){"
      "fetch('/api/wifi_scan').then(r=>r.json()).then(d=>{"
        "let ul=document.getElementById('nets'); ul.innerHTML='';"
        "d.networks.forEach(n=>{"
          "let li=document.createElement('li');"
          "li.textContent=n.ssid+' (RSSI '+n.rssi+')';"
          "li.onclick=function(){document.getElementById('ssid').value=n.ssid;};"
          "ul.appendChild(li);"
        "});"
      "});"
    "}"
    "function setWifi(){"
      "let data=new URLSearchParams();"
      "data.append('ssid', document.getElementById('ssid').value);"
      "data.append('pass', document.getElementById('pass').value);"
      "fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data.toString()})"
        ".then(_=>{alert('Сохранено, устройство перезагрузится');});"
    "}"
    "</script>"
    "</body></html>"
  );
  server.send(200, "text/html", html);
}

// ===== JSON =====

String WebInterface::buildSensorsJson() {
  String j = "{";
  j += "\"airTemperature\":"  + String(isnan(g_sensorData.airTemperature)?0:g_sensorData.airTemperature, 1) + ",";
  j += "\"airHumidity\":"     + String(isnan(g_sensorData.airHumidity)?0:g_sensorData.airHumidity, 1) + ",";
  j += "\"airPressure\":"     + String(isnan(g_sensorData.airPressure)?0:g_sensorData.airPressure, 1) + ",";
  j += "\"soilTemperature\":" + String(isnan(g_sensorData.soilTemperature)?0:g_sensorData.soilTemperature, 1) + ",";
  j += "\"soilMoisture\":"    + String(isnan(g_sensorData.soilMoisture)?0:g_sensorData.soilMoisture, 1) + ",";
  j += "\"lightLevelLux\":"   + String(isnan(g_sensorData.lightLevelLux)?0:g_sensorData.lightLevelLux, 1) + ",";
  j += "\"pumpOn\":"          + String(g_sensorData.pumpOn ? "true" : "false") + ",";
  j += "\"fanOn\":"           + String(g_sensorData.fanOn ? "true" : "false") + ",";
  j += "\"lightOn\":"         + String(g_sensorData.lightOn ? "true" : "false") + ",";
  j += "\"doorOpen\":"        + String(g_sensorData.doorOpen ? "true" : "false");
  j += "}";
  return j;
}

String WebInterface::buildSettingsJson() {
  String j = "{";
  j += "\"comfortTempMin\":"      + String(g_settings.comfortTempMin,1) + ",";
  j += "\"comfortTempMax\":"      + String(g_settings.comfortTempMax,1) + ",";
  j += "\"comfortHumMin\":"       + String(g_settings.comfortHumMin,1) + ",";
  j += "\"comfortHumMax\":"       + String(g_settings.comfortHumMax,1) + ",";
  j += "\"soilMoistureSetpoint\":"+ String(g_settings.soilMoistureSetpoint,1) + ",";
  j += "\"lightLuxMin\":"         + String(g_settings.lightLuxMin,1) + ",";
  j += "\"lightMode\":"           + String(g_settings.lightMode) + ",";
  j += "\"cropProfile\":"         + String(g_settings.cropProfile) + ",";
  j += "\"airTempOffset\":"       + String(g_settings.airTempOffset,1) + ",";
  j += "\"airHumOffset\":"        + String(g_settings.airHumOffset,1) + ",";
  j += "\"soilTempOffset\":"      + String(g_settings.soilTempOffset,1) + ",";
  j += "\"automationEnabled\":"   + String(g_settings.automationEnabled ? "true" : "false") + ",";
  j += "\"allowNightLight\":"     + String(g_settings.allowNightLight ? "true" : "false");
  j += "}";
  return j;
}

String WebInterface::buildDiagnosticsJson() {
  g_devices.refreshI2CDevices();
  int soilRaw = analogRead(Pins::SOIL_MOISTURE);

  String j = "{";
  j += "\"bme\":{";
    j += "\"has\":"     + String(g_deviceConfig.hasBME280 ? "true" : "false") + ",";
    j += "\"healthy\":" + String(g_deviceConfig.bmeHealthy ? "true" : "false") + ",";
    j += "\"addr\":"    + String(g_deviceConfig.bmeAddr);
  j += "},";

  j += "\"bh\":{";
    j += "\"has\":"     + String(g_deviceConfig.hasBH1750 ? "true" : "false") + ",";
    j += "\"healthy\":" + String(g_deviceConfig.bhHealthy ? "true" : "false") + ",";
    j += "\"addr\":"    + String(g_deviceConfig.bhAddr);
  j += "},";

  j += "\"soil\":{";
    j += "\"has\":"     + String(g_deviceConfig.hasSoilSensor ? "true" : "false") + ",";
    j += "\"healthy\":" + String(g_deviceConfig.soilHealthy ? "true" : "false") + ",";
    j += "\"raw\":"     + String(soilRaw);
  j += "},";

  j += "\"led\":{";
    j += "\"has\":" + String(g_deviceConfig.hasLEDMatrix ? "true" : "false");
  j += "},";

  j += "\"servo\":{";
    j += "\"has\":" + String(g_deviceConfig.hasServo ? "true" : "false");
  j += "}";

  j += "}";
  return j;
}

// ===== API =====

void WebInterface::handleSensors() {
  server.send(200, "application/json", buildSensorsJson());
}

void WebInterface::handleSettingsGet() {
  server.send(200, "application/json", buildSettingsJson());
}

void WebInterface::handleSettingsPost() {
  auto getF = [&](const char *name, float current)->float {
    if (!server.hasArg(name)) return current;
    return server.arg(name).toFloat();
  };
  auto getB = [&](const char *name, bool current)->bool {
    if (!server.hasArg(name)) return current;
    return server.arg(name) == "1";
  };
  auto getU8 = [&](const char *name, uint8_t current)->uint8_t {
    if (!server.hasArg(name)) return current;
    return (uint8_t)server.arg(name).toInt();
  };

  g_settings.comfortTempMin       = getF("comfortTempMin", g_settings.comfortTempMin);
  g_settings.comfortTempMax       = getF("comfortTempMax", g_settings.comfortTempMax);
  g_settings.comfortHumMin        = getF("comfortHumMin",  g_settings.comfortHumMin);
  g_settings.comfortHumMax        = getF("comfortHumMax",  g_settings.comfortHumMax);
  g_settings.soilMoistureSetpoint = getF("soilMoistureSetpoint", g_settings.soilMoistureSetpoint);
  g_settings.lightLuxMin          = getF("lightLuxMin", g_settings.lightLuxMin);
  g_settings.lightMode            = getU8("lightMode", g_settings.lightMode);
  g_settings.automationEnabled    = getB("automationEnabled", g_settings.automationEnabled);
  g_settings.allowNightLight      = getB("allowNightLight", g_settings.allowNightLight);

  g_settings.airTempOffset  = getF("airTempOffset",  g_settings.airTempOffset);
  g_settings.airHumOffset   = getF("airHumOffset",   g_settings.airHumOffset);
  g_settings.soilTempOffset = getF("soilTempOffset", g_settings.soilTempOffset);

  uint8_t newProfile = getU8("cropProfile", g_settings.cropProfile);
  if (newProfile != g_settings.cropProfile) {
    applyCropProfile(newProfile, g_settings);
  }

  g_eeprom.saveSettings(g_settings);
  server.send(200, "text/plain", "OK");
}

void WebInterface::handleControl() {
  String dev = server.hasArg("dev") ? server.arg("dev") : "";
  int on     = server.hasArg("on")  ? server.arg("on").toInt() : 0;
  bool state = (on != 0);

  if (dev == "pump") {
    g_devices.setPump(state, state ? Constants::PUMP_PULSE_MS : 0);
  } else if (dev == "fan") {
    g_devices.setFan(state);
  } else if (dev == "light") {
    g_devices.setLight(state);
  } else if (dev == "door") {
    g_devices.setDoorAngle(state ? Constants::SERVO_OPEN_ANGLE : Constants::SERVO_CLOSED_ANGLE);
  }

  server.send(200, "text/plain", "OK");
}

void WebInterface::handleCalibrate() {
  if (!server.hasArg("target")) {
    server.send(400, "text/plain", "missing target");
    return;
  }
  String t = server.arg("target");
  if (t == "air") {
    g_devices.calibrateSoilSensor(false);
  } else if (t == "water") {
    g_devices.calibrateSoilSensor(true);
  }
  server.send(200, "text/plain", "OK");
}

void WebInterface::handleDiagnosticsApi() {
  server.send(200, "application/json", buildDiagnosticsJson());
}

void WebInterface::handleWifiScan() {
  int n = WiFi.scanNetworks();
  String j = "{ \"networks\": [";
  for (int i = 0; i < n; i++) {
    if (i > 0) j += ",";
    j += "{";
    j += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    j += "\"rssi\":" + String(WiFi.RSSI(i));
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void WebInterface::handleWifiSet() {
  if (!server.hasArg("ssid") || !server.hasArg("pass")) {
    server.send(400, "text/plain", "ssid/pass required");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  ssid.toCharArray(g_settings.wifiSSID, sizeof(g_settings.wifiSSID));
  pass.toCharArray(g_settings.wifiPassword, sizeof(g_settings.wifiPassword));

  g_eeprom.saveSettings(g_settings);

  server.send(200, "text/plain", "OK (reboot required)");
  delay(500);
  ESP.restart();
}