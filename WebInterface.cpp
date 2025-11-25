// WebInterface.cpp
#include "WebInterface.h"

WebInterface g_web;

void WebInterface::begin() {
  setupWiFi();

  server.on("/",             HTTP_GET,  [this]() { if (!ensureAuth()) return; handleRoot(); });
  server.on("/api/sensors",  HTTP_GET,  [this]() { if (!ensureAuth()) return; handleSensors(); });
  server.on("/api/settings", HTTP_GET,  [this]() { if (!ensureAuth()) return; handleSettingsGet(); });
  server.on("/api/settings", HTTP_POST, [this]() { if (!ensureAuth()) return; handleSettingsPost(); });
  server.on("/api/control",  HTTP_POST, [this]() { if (!ensureAuth()) return; handleControl(); });
  server.on("/api/diagnostics", HTTP_GET, [this]() { if (!ensureAuth()) return; handleDiagnosticsApi(); });
  server.on("/api/wifi_scan",   HTTP_GET,  [this]() { if (!ensureAuth()) return; handleWifiScan(); });
  server.on("/api/wifi_set",    HTTP_POST, [this]() { if (!ensureAuth()) return; handleWifiSet(); });

  server.onNotFound([this]() { handleNotFound(); });

  server.begin();
  Serial.println(F("🌐 Web сервер запущен на порту 80"));
}

void WebInterface::loop() {
  server.handleClient();
}

void WebInterface::setupWiFi() {
  // Пытаемся подключиться как STA по сохранённым настройкам
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_settings.wifiSSID, g_settings.wifiPassword);

  Serial.printf("📶 Подключаемся к Wi-Fi \"%s\"...\n", g_settings.wifiSSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("✅ Wi-Fi STA IP: "));
    Serial.println(WiFi.localIP());
  } else {
    // Переходим в режим точки доступа для первичной настройки
    Serial.println(F("⚠️ Не удалось подключиться, поднимаем точку доступа..."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("YotikM2-Setup", "YotikM2pass");
    Serial.print(F("📡 AP IP: "));
    Serial.println(WiFi.softAPIP());
  }
}

bool WebInterface::ensureAuth() {
  if (!server.authenticate(WEB_USER, WEB_PASS)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// ===== HTML (современный интерфейс) =====

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>ЙоТик M2 — Умная теплица</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root {
    font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    color-scheme: dark;
  }
  body {
    margin: 0;
    padding: 0;
    min-height: 100vh;
    background: radial-gradient(circle at top, #2c3e50, #0b1020);
    color: #f5f5f5;
    display: flex;
    justify-content: center;
    align-items: flex-start;
  }
  .container {
    width: 100%;
    max-width: 1080px;
    padding: 24px 16px 40px;
  }
  .header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 16px;
    gap: 12px;
  }
  .title {
    font-size: 1.6rem;
    font-weight: 700;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .title span.logo {
    display: inline-flex;
    width: 32px;
    height: 32px;
    border-radius: 12px;
    background: linear-gradient(135deg, #27ae60, #8e44ad);
    align-items: center;
    justify-content: center;
    font-size: 1.1rem;
  }
  .badge {
    padding: 4px 10px;
    border-radius: 999px;
    background: rgba(255,255,255,0.05);
    font-size: 0.75rem;
    border: 1px solid rgba(255,255,255,0.08);
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
    gap: 16px;
  }
  .card {
    background: rgba(17,24,39,0.9);
    border-radius: 18px;
    padding: 16px 18px;
    box-shadow: 0 18px 45px rgba(0,0,0,0.45);
    border: 1px solid rgba(255,255,255,0.04);
    backdrop-filter: blur(18px);
  }
  .card h3 {
    margin: 0 0 10px;
    font-size: 1.05rem;
    font-weight: 600;
  }
  .metrics {
    display: grid;
    grid-template-columns: repeat(2, minmax(0,1fr));
    gap: 10px;
  }
  .metric {
    padding: 8px 10px;
    border-radius: 12px;
    background: rgba(255,255,255,0.02);
    border: 1px solid rgba(255,255,255,0.06);
    font-size: 0.88rem;
  }
  .metric .label {
    opacity: 0.75;
    font-size: 0.78rem;
  }
  .metric .value {
    font-size: 1rem;
    font-weight: 600;
  }
  .metric .value small {
    opacity: 0.7;
    font-weight: 400;
    margin-left: 4px;
  }
  .metric.ok    { border-color: rgba(52, 211, 153,0.7); }
  .metric.warn  { border-color: rgba(250, 204, 21,0.7); }
  .metric.alarm { border-color: rgba(248, 113, 113,0.8); }

  .control-group {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    margin-bottom: 10px;
    align-items: center;
  }
  .control-group span {
    font-size: 0.85rem;
    opacity: 0.75;
    min-width: 80px;
  }
  button {
    border-radius: 999px;
    border: none;
    padding: 7px 14px;
    font-size: 0.85rem;
    font-weight: 500;
    cursor: pointer;
    background: rgba(255,255,255,0.06);
    color: #f5f5f5;
    transition: transform 0.08s ease, box-shadow 0.12s ease, background 0.12s ease;
  }
  button.primary {
    background: linear-gradient(135deg,#22c55e,#16a34a);
  }
  button.danger {
    background: linear-gradient(135deg,#ef4444,#b91c1c);
  }
  button.outline {
    background: transparent;
    border: 1px solid rgba(255,255,255,0.12);
  }
  button:hover {
    transform: translateY(-1px);
    box-shadow: 0 10px 25px rgba(0,0,0,0.25);
  }
  button:active {
    transform: translateY(0);
    box-shadow: none;
  }

  form.settings-form {
    display: grid;
    grid-template-columns: repeat(2,minmax(0,1fr));
    gap: 10px;
    margin-bottom: 10px;
  }
  .field {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .field label {
    font-size: 0.76rem;
    opacity: 0.8;
  }
  .field input, .field select {
    border-radius: 10px;
    border: 1px solid rgba(255,255,255,0.08);
    background: rgba(15,23,42,0.9);
    color: #f5f5f5;
    font-size: 0.86rem;
    padding: 6px 9px;
  }
  .footer {
    margin-top: 18px;
    font-size: 0.75rem;
    opacity: 0.6;
    display: flex;
    justify-content: space-between;
    gap: 8px;
    flex-wrap: wrap;
  }
  .chip {
    padding: 3px 8px;
    border-radius: 999px;
    border: 1px solid rgba(148,163,184,0.7);
    font-size: 0.7rem;
  }
  .status-dot {
    width: 10px;
    height: 10px;
    border-radius: 999px;
    background: #22c55e;
    margin-right: 6px;
  }
  .status-row {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 0.8rem;
  }
  .wifi-note {
    font-size: 0.75rem;
    opacity: 0.75;
    margin-top: 4px;
  }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="title">
      <span class="logo">🌱</span>
      <span>ЙоТик M2 — Умная теплица</span>
    </div>
    <div class="badge">Веб-панель · v2.1</div>
  </div>

  <div class="grid">
    <!-- Состояние -->
    <div class="card">
      <h3>Текущее состояние</h3>
      <div class="metrics" id="metrics">
        <div class="metric" id="m-airT">
          <div class="label">Температура воздуха</div>
          <div class="value">--<small>°C</small></div>
        </div>
        <div class="metric" id="m-airH">
          <div class="label">Влажность воздуха</div>
          <div class="value">--<small>%</small></div>
        </div>
        <div class="metric" id="m-soilW">
          <div class="label">Влажность почвы</div>
          <div class="value">--<small>%</small></div>
        </div>
        <div class="metric" id="m-lux">
          <div class="label">Освещённость</div>
          <div class="value">--<small>lux</small></div>
        </div>
      </div>
    </div>

    <!-- Управление -->
    <div class="card">
      <h3>Ручное управление</h3>
      <div class="control-group">
        <span>Насос</span>
        <button class="primary" onclick="ctrl('pump','pulse')">Импульс</button>
        <button class="outline" onclick="ctrl('pump','on')">ВКЛ</button>
        <button class="outline" onclick="ctrl('pump','off')">ВЫКЛ</button>
      </div>
      <div class="control-group">
        <span>Свет</span>
        <button class="primary" onclick="ctrl('light','on')">ВКЛ</button>
        <button class="outline" onclick="ctrl('light','off')">ВЫКЛ</button>
      </div>
      <div class="control-group">
        <span>Вентиляция</span>
        <button class="primary" onclick="ctrl('fan','on')">ВКЛ</button>
        <button class="outline" onclick="ctrl('fan','off')">ВЫКЛ</button>
      </div>
      <div class="control-group">
        <span>Дверь</span>
        <button class="primary" onclick="ctrl('door','open')">ОТКРЫТЬ</button>
        <button class="outline" onclick="ctrl('door','half')">ПРИОТКРЫТЬ</button>
        <button class="outline" onclick="ctrl('door','close')">ЗАКРЫТЬ</button>
      </div>
      <div class="control-group">
        <span>Автоматика</span>
        <button class="primary" onclick="ctrl('auto','on')">ВКЛ</button>
        <button class="outline" onclick="ctrl('auto','off')">ВЫКЛ</button>
      </div>
    </div>

    <!-- Настройки -->
    <div class="card">
      <h3>Настройки климата и полива</h3>
      <form class="settings-form" id="settingsForm" onsubmit="saveSettings();return false;">
        <div class="field">
          <label>Temp мин</label>
          <input type="number" step="0.1" id="comfortTempMin">
        </div>
        <div class="field">
          <label>Temp макс</label>
          <input type="number" step="0.1" id="comfortTempMax">
        </div>
        <div class="field">
          <label>Hum мин</label>
          <input type="number" step="0.1" id="comfortHumMin">
        </div>
        <div class="field">
          <label>Hum макс</label>
          <input type="number" step="0.1" id="comfortHumMax">
        </div>
        <div class="field">
          <label>Порог почвы %</label>
          <input type="number" step="1" id="soilMoistureSetpoint">
        </div>
        <div class="field">
          <label>Гистерезис почвы %</label>
          <input type="number" step="1" id="soilMoistureHysteresis">
        </div>
        <div class="field">
          <label>Порог света lux</label>
          <input type="number" step="1" id="lightLuxMin">
        </div>
        <div class="field">
          <label>Окно полива (от/до)</label>
          <div style="display:flex;gap:4px;">
            <input type="number" min="0" max="23" id="wateringStartHour" style="width:50%;">
            <input type="number" min="0" max="23" id="wateringEndHour" style="width:50%;">
          </div>
        </div>
        <div class="field">
          <label>Ограничение света (час)</label>
          <input type="number" min="0" max="23" id="lightCutoffHour">
        </div>
        <div class="field">
          <label>Режим климата</label>
          <select id="climateMode">
            <option value="0">Eco</option>
            <option value="1">Normal</option>
            <option value="2">Aggressive</option>
          </select>
        </div>
      </form>
      <button class="primary" onclick="saveSettings()">💾 Сохранить</button>
    </div>

    <!-- Диагностика / Wi-Fi -->
    <div class="card">
      <h3>Диагностика и сеть</h3>
      <pre id="diag" style="font-size:0.75rem;max-height:150px;overflow:auto;background:rgba(15,23,42,0.9);padding:8px;border-radius:10px;border:1px solid rgba(148,163,184,0.4);"></pre>

      <div class="control-group">
        <span>Wi-Fi</span>
        <button class="outline" onclick="scanWifi()">Сканировать</button>
      </div>
      <div id="wifiList" style="font-size:0.78rem;opacity:0.85;margin-bottom:8px;"></div>

      <div class="field">
        <label>SSID сети</label>
        <input type="text" id="wifiSsid" placeholder="Имя Wi-Fi сети">
      </div>
      <div class="field">
        <label>Пароль</label>
        <input type="password" id="wifiPass" placeholder="Пароль Wi-Fi">
      </div>
      <div class="control-group" style="margin-top:8px;">
        <button class="primary" onclick="saveWifi()">Подключить к Wi-Fi</button>
      </div>
      <div class="wifi-note">
        При работе в режиме точки доступа (YotikM2-Setup) введите здесь домашний Wi-Fi, сохраните — устройство попытается подключиться к нему.
      </div>
    </div>
  </div>

  <div class="footer">
    <div class="status-row">
      <span class="status-dot" id="statusDot"></span>
      <span id="statusText">Обновление данных...</span>
    </div>
    <div class="chip">Логин/пароль панели: admin / greenhouse</div>
  </div>
</div>

<script>
let lastSensors = null;

function classForMetric(id, value) {
  if (value === null) return '';
  if (id === 'm-airT') {
    if (value < 10 || value > 40) return 'alarm';
    if (value < 15 || value > 35) return 'warn';
    return 'ok';
  }
  if (id === 'm-airH') {
    if (value < 20 || value > 90) return 'alarm';
    if (value < 30 || value > 80) return 'warn';
    return 'ok';
  }
  if (id === 'm-soilW') {
    if (value < 15 || value > 95) return 'alarm';
    if (value < 25 || value > 90) return 'warn';
    return 'ok';
  }
  if (id === 'm-lux') {
    return '';
  }
  return '';
}

function updateMetrics(s) {
  lastSensors = s;
  const map = {
    'm-airT': {v: s.airTemperature, u:'°C'},
    'm-airH': {v: s.airHumidity,    u:'%'},
    'm-soilW':{v: s.soilMoisture,   u:'%'},
    'm-lux':  {v: s.lightLevelLux,  u:'lux'}
  };
  Object.keys(map).forEach(id => {
    const el  = document.getElementById(id);
    const val = map[id].v;
    const u   = map[id].u;
    if (!el) return;
    const vEl = el.querySelector('.value');
    if (val === null || isNaN(val)) {
      vEl.innerHTML = '--<small>'+u+'</small>';
      el.className = 'metric';
    } else {
      vEl.innerHTML = val.toFixed(1)+'<small>'+u+'</small>';
      const cls = classForMetric(id, val);
      el.className = 'metric'+(cls ? ' '+cls : '');
    }
  });

  const dot = document.getElementById('statusDot');
  const txt = document.getElementById('statusText');
  if (s.automationEnabled) {
    dot.style.background = '#22c55e';
    txt.textContent = 'Автоматика: ВКЛ';
  } else {
    dot.style.background = '#f97316';
    txt.textContent = 'Автоматика: ВЫКЛ';
  }
}

async function fetchJson(url, opts) {
  const res = await fetch(url, opts || {});
  if (!res.ok) throw new Error('HTTP '+res.status);
  return await res.json();
}

async function loadSensors() {
  try {
    const data = await fetchJson('/api/sensors');
    updateMetrics(data);
  } catch(e) {
    console.error(e);
  }
}

async function loadSettings() {
  try {
    const s = await fetchJson('/api/settings');
    const set = (id,val)=>{ const el=document.getElementById(id); if(el) el.value = val; };
    set('comfortTempMin', s.comfortTempMin);
    set('comfortTempMax', s.comfortTempMax);
    set('comfortHumMin',  s.comfortHumMin);
    set('comfortHumMax',  s.comfortHumMax);
    set('soilMoistureSetpoint', s.soilMoistureSetpoint);
    set('soilMoistureHysteresis', s.soilMoistureHysteresis);
    set('lightLuxMin', s.lightLuxMin);
    set('wateringStartHour', s.wateringStartHour);
    set('wateringEndHour',   s.wateringEndHour);
    set('lightCutoffHour',   s.lightCutoffHour);
    set('climateMode',       s.climateMode);
  } catch(e) {
    console.error(e);
  }
}

async function saveSettings() {
  const ids = ['comfortTempMin','comfortTempMax','comfortHumMin','comfortHumMax',
               'soilMoistureSetpoint','soilMoistureHysteresis','lightLuxMin',
               'wateringStartHour','wateringEndHour','lightCutoffHour','climateMode'];
  const body = {};
  ids.forEach(id => {
    const el = document.getElementById(id);
    if (!el) return;
    body[id] = el.value;
  });

  try {
    await fetch('/api/settings', {
      method: 'POST',
      headers: { 'Content-Type':'application/json' },
      body: JSON.stringify(body)
    });
    alert('Настройки сохранены');
    loadSettings();
  } catch(e) {
    console.error(e);
    alert('Ошибка сохранения');
  }
}

async function ctrl(device, action) {
  try {
    await fetch('/api/control', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ device, action })
    });
  } catch(e) {
    console.error(e);
  }
}

async function scanWifi() {
  const listEl = document.getElementById('wifiList');
  listEl.textContent = 'Сканирование...';
  try {
    const data = await fetchJson('/api/wifi_scan');
    if (!data.networks || !data.networks.length) {
      listEl.textContent = 'Сети не найдены';
      return;
    }
    listEl.innerHTML = data.networks.map(n => `• ${n.ssid} (${n.rssi} dBm)`).join('<br>');
  } catch(e) {
    console.error(e);
    listEl.textContent = 'Ошибка сканирования';
  }
}

async function saveWifi() {
  const ssid = document.getElementById('wifiSsid').value.trim();
  const pass = document.getElementById('wifiPass').value.trim();
  if (!ssid) {
    alert('Введите SSID сети');
    return;
  }
  try {
    const res = await fetch('/api/wifi_set', {
      method: 'POST',
      headers: { 'Content-Type':'application/json' },
      body: JSON.stringify({ ssid, password: pass })
    });
    const txt = await res.text();
    try {
      const j = JSON.parse(txt);
      alert(j.message || 'Настройки сохранены. Устройство перезапускает Wi-Fi.');
    } catch {
      alert('Ответ: '+txt);
    }
  } catch(e) {
    console.error(e);
    alert('Ошибка отправки настроек Wi-Fi');
  }
}

async function loadDiag() {
  try {
    const data = await fetchJson('/api/diagnostics');
    const pre  = document.getElementById('diag');
    pre.textContent = data.text;
  } catch(e) {
    console.error(e);
  }
}

setInterval(loadSensors, 4000);
setInterval(loadDiag,    15000);

loadSensors();
loadSettings();
loadDiag();
</script>
</body>
</html>
)rawliteral";

void WebInterface::handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void WebInterface::handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ===== API =====

String WebInterface::buildSensorsJson() {
  String j;
  j.reserve(256);
  j += "{";
  j += "\"airTemperature\":"  + String(isnan(g_sensorData.airTemperature)?0:g_sensorData.airTemperature,1) + ",";
  j += "\"airHumidity\":"     + String(isnan(g_sensorData.airHumidity)?0:g_sensorData.airHumidity,1) + ",";
  j += "\"soilMoisture\":"    + String(isnan(g_sensorData.soilMoisture)?0:g_sensorData.soilMoisture,1) + ",";
  j += "\"lightLevelLux\":"   + String(isnan(g_sensorData.lightLevelLux)?0:g_sensorData.lightLevelLux,1) + ",";
  j += "\"pumpOn\":"          + String(g_sensorData.pumpOn ? "true":"false") + ",";
  j += "\"fanOn\":"           + String(g_sensorData.fanOn  ? "true":"false") + ",";
  j += "\"lightOn\":"         + String(g_sensorData.lightOn ? "true":"false") + ",";
  j += "\"doorOpen\":"        + String(g_sensorData.doorOpen ? "true":"false") + ",";
  j += "\"automationEnabled\":"+ String(g_settings.automationEnabled ? "true":"false");
  j += "}";
  return j;
}

void WebInterface::handleSensors() {
  server.send(200, "application/json", buildSensorsJson());
}

String WebInterface::buildSettingsJson() {
  String j;
  j.reserve(256);
  j += "{";
  j += "\"comfortTempMin\":"       + String(g_settings.comfortTempMin,1) + ",";
  j += "\"comfortTempMax\":"       + String(g_settings.comfortTempMax,1) + ",";
  j += "\"comfortHumMin\":"        + String(g_settings.comfortHumMin,1) + ",";
  j += "\"comfortHumMax\":"        + String(g_settings.comfortHumMax,1) + ",";
  j += "\"soilMoistureSetpoint\":"+ String(g_settings.soilMoistureSetpoint,1) + ",";
  j += "\"soilMoistureHysteresis\":"+String(g_settings.soilMoistureHysteresis,1) + ",";
  j += "\"lightLuxMin\":"          + String(g_settings.lightLuxMin,1) + ",";
  j += "\"wateringStartHour\":"    + String(g_settings.wateringStartHour) + ",";
  j += "\"wateringEndHour\":"      + String(g_settings.wateringEndHour) + ",";
  j += "\"lightCutoffHour\":"      + String(g_settings.lightCutoffHour) + ",";
  j += "\"climateMode\":"          + String(g_settings.climateMode);
  j += "}";
  return j;
}

void WebInterface::handleSettingsGet() {
  server.send(200, "application/json", buildSettingsJson());
}

void WebInterface::handleSettingsPost() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Expected JSON body");
    return;
  }
  String body = server.arg("plain");

  auto getNumber = [&](const String &key, float current)->float {
    int idx = body.indexOf("\"" + key + "\"");
    if (idx < 0) return current;
    idx = body.indexOf(":", idx);
    if (idx < 0) return current;
    int end = body.indexOf(",", idx);
    if (end < 0) end = body.indexOf("}", idx);
    if (end < 0) return current;
    String sub = body.substring(idx+1, end);
    sub.trim();
    return sub.toFloat();
  };

  auto getInt = [&](const String &key, int current)->int {
    return (int)getNumber(key, current);
  };

  g_settings.comfortTempMin        = getNumber("comfortTempMin", g_settings.comfortTempMin);
  g_settings.comfortTempMax        = getNumber("comfortTempMax", g_settings.comfortTempMax);
  g_settings.comfortHumMin         = getNumber("comfortHumMin",  g_settings.comfortHumMin);
  g_settings.comfortHumMax         = getNumber("comfortHumMax",  g_settings.comfortHumMax);
  g_settings.soilMoistureSetpoint  = getNumber("soilMoistureSetpoint", g_settings.soilMoistureSetpoint);
  g_settings.soilMoistureHysteresis= getNumber("soilMoistureHysteresis", g_settings.soilMoistureHysteresis);
  g_settings.lightLuxMin           = getNumber("lightLuxMin", g_settings.lightLuxMin);
  g_settings.wateringStartHour     = (uint8_t)constrain(getInt("wateringStartHour", g_settings.wateringStartHour),0,23);
  g_settings.wateringEndHour       = (uint8_t)constrain(getInt("wateringEndHour",   g_settings.wateringEndHour),0,23);
  g_settings.lightCutoffHour       = (uint8_t)constrain(getInt("lightCutoffHour",   g_settings.lightCutoffHour),0,23);
  g_settings.climateMode           = (uint8_t)constrain(getInt("climateMode",       g_settings.climateMode),0,2);

  g_eeprom.saveSettings(g_settings);
  server.send(200, "text/plain", "OK");
}

void WebInterface::handleControl() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Expected JSON body");
    return;
  }
  String body = server.arg("plain");

  auto getStr = [&](const String &key)->String {
    int idx = body.indexOf("\"" + key + "\"");
    if (idx < 0) return "";
    idx = body.indexOf(":", idx);
    if (idx < 0) return "";
    int q1 = body.indexOf("\"", idx+1);
    int q2 = body.indexOf("\"", q1+1);
    if (q1 < 0 || q2 < 0) return "";
    return body.substring(q1+1, q2);
  };

  String device = getStr("device");
  String action = getStr("action");

  if (device == "pump") {
    if      (action == "pulse") g_devices.setPump(true, 1000);
    else if (action == "on")    g_devices.setPump(true, 0);
    else if (action == "off")   g_devices.setPump(false);
  } else if (device == "light") {
    if      (action == "on")    g_devices.setLight(true);
    else if (action == "off")   g_devices.setLight(false);
  } else if (device == "fan") {
    if      (action == "on")    g_devices.setFan(true);
    else if (action == "off")   g_devices.setFan(false);
  } else if (device == "door") {
    if      (action == "open")  g_devices.setDoorAngle(Constants::SERVO_OPEN_ANGLE);
    else if (action == "half")  g_devices.setDoorAngle(Constants::SERVO_HALF_ANGLE);
    else if (action == "close") g_devices.setDoorAngle(Constants::SERVO_CLOSED_ANGLE);
  } else if (device == "auto") {
    if      (action == "on")  g_settings.automationEnabled = true;
    else if (action == "off") g_settings.automationEnabled = false;
  }

  server.send(200, "text/plain", "OK");
}

String WebInterface::buildDiagnosticsJson() {
  String txt;
  txt.reserve(256);

  txt += "Wi-Fi: ";
  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {
    txt += "STA ";
    txt += WiFi.SSID();
    txt += " (";
    txt += WiFi.localIP().toString();
    txt += ")\n";
  } else if (WiFi.getMode() == WIFI_AP) {
    txt += "AP ";
    txt += WiFi.softAPIP().toString();
    txt += "\n";
  } else {
    txt += "нет соединения\n";
  }

  txt += "Климат: T=";
  txt += String(g_sensorData.airTemperature,1);
  txt += "C H=";
  txt += String(g_sensorData.airHumidity,1);
  txt += "%\n";

  txt += "Почва: W=";
  txt += String(g_sensorData.soilMoisture,1);
  txt += "%\n";

  txt += "Свет: L=";
  txt += String(g_sensorData.lightLevelLux,1);
  txt += " lux\n";

  txt += "Устройства: насос=";
  txt += (g_sensorData.pumpOn ? "ВКЛ" : "ВЫКЛ");
  txt += ", вентилятор=";
  txt += (g_sensorData.fanOn ? "ВКЛ":"ВЫКЛ");
  txt += ", свет=";
  txt += (g_sensorData.lightOn ? "ВКЛ":"ВЫКЛ");
  txt += "\n";

  String j;
  j.reserve(txt.length() + 20);
  j += "{";
  j += "\"text\":\"";

  for (size_t i=0;i<txt.length();++i) {
    char c = txt[i];
    if (c == '\n')      j += "\\n";
    else if (c == '\"') j += "\\\"";
    else                j += c;
  }
  j += "\"}";
  return j;
}

void WebInterface::handleDiagnosticsApi() {
  server.send(200, "application/json", buildDiagnosticsJson());
}

void WebInterface::handleWifiScan() {
  int n = WiFi.scanNetworks();
  String j;
  j.reserve(128);
  j += "{\"networks\":[";
  for (int i=0;i<n;++i) {
    if (i>0) j += ",";
    j += "{";
    j += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    j += "\"rssi\":" + String(WiFi.RSSI(i));
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void WebInterface::handleWifiSet() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"expected JSON body\"}");
    return;
  }

  String body = server.arg("plain");

  auto getString = [&](const String &key, const String &current)->String {
    int idx = body.indexOf("\"" + key + "\"");
    if (idx < 0) return current;
    idx = body.indexOf(":", idx);
    if (idx < 0) return current;
    int q1 = body.indexOf("\"", idx+1);
    int q2 = body.indexOf("\"", q1+1);
    if (q1 < 0 || q2 < 0) return current;
    String sub = body.substring(q1+1, q2);
    sub.trim();
    return sub;
  };

  String ssid = getString("ssid", "");
  String pass = getString("password", "");

  if (ssid.length() == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"SSID пустой\"}");
    return;
  }

  // Сохраняем в настройки
  ssid.toCharArray(g_settings.wifiSSID, sizeof(g_settings.wifiSSID));
  pass.toCharArray(g_settings.wifiPassword, sizeof(g_settings.wifiPassword));
  g_eeprom.saveSettings(g_settings);

  // Переинициализация Wi-Fi в STA
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_settings.wifiSSID, g_settings.wifiPassword);

  server.send(200, "application/json",
              "{\"ok\":true,\"message\":\"Настройки сохранены. Устройство пытается подключиться к Wi-Fi. Если пропала точка доступа — ищите его в вашей сети.\"}");
}