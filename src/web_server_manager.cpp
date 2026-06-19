// =================================================================
//  web_server_manager.cpp
// =================================================================
#include "web_server_manager.h"

// =================================================================
//  Embedded HTML dashboard (stored in flash via PROGMEM)
//  Features:
//    - Live sensor cards (auto-refresh every 2s via fetch)
//    - Actuator cards with AUTO/MANUAL toggle + ON/OFF button
//    - Fault banner with red alert
//    - Mobile-responsive grid
// =================================================================
static const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Greenhouse</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
     background:#0f1117;color:#e2e8f0;min-height:100vh}
header{background:#1a1d2e;border-bottom:1px solid #2d3148;padding:14px 20px;
       display:flex;justify-content:space-between;align-items:center}
header h1{font-size:18px;font-weight:600;color:#7c83ff}
header span{font-size:12px;color:#64748b}
.container{max-width:960px;margin:0 auto;padding:16px}
.fault-banner{background:#7f1d1d;border:1px solid #ef4444;border-radius:10px;
  padding:14px 18px;margin-bottom:16px;display:none;align-items:center;gap:10px}
.fault-banner.active{display:flex}
.fault-icon{font-size:22px}
.fault-text h3{font-size:14px;font-weight:600;color:#fca5a5;margin-bottom:2px}
.fault-text p{font-size:12px;color:#f87171}
h2{font-size:13px;font-weight:600;color:#94a3b8;text-transform:uppercase;
   letter-spacing:.06em;margin:20px 0 10px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(175px,1fr));gap:12px}
.card{background:#1a1d2e;border:1px solid #2d3148;border-radius:12px;padding:16px}
.card-label{font-size:11px;color:#64748b;text-transform:uppercase;letter-spacing:.05em;margin-bottom:6px}
.card-value{font-size:28px;font-weight:600;color:#e2e8f0;line-height:1}
.card-unit{font-size:13px;color:#94a3b8;margin-left:3px}
.card-sub{font-size:11px;color:#64748b;margin-top:5px}
.bar-wrap{background:#2d3148;border-radius:4px;height:6px;margin-top:8px;overflow:hidden}
.bar-fill{height:100%;border-radius:4px;transition:width .5s ease}
.bar-blue{background:#3b82f6}
.bar-green{background:#22c55e}
.bar-orange{background:#f97316}
.bar-red{background:#ef4444}
.status-ok{color:#22c55e;font-weight:600}
.status-warn{color:#f97316;font-weight:600}
.status-err{color:#ef4444;font-weight:600}
/* Actuator cards */
.act-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:12px}
.act-card{background:#1a1d2e;border:1px solid #2d3148;border-radius:12px;padding:14px}
.act-name{font-size:12px;font-weight:600;color:#94a3b8;margin-bottom:10px;text-transform:uppercase}
.act-state{font-size:20px;font-weight:700;margin-bottom:10px}
.act-state.on{color:#22c55e}
.act-state.off{color:#ef4444}
.mode-row{display:flex;gap:6px;margin-bottom:8px}
.mode-btn{flex:1;padding:5px 0;border-radius:6px;border:1px solid #2d3148;
  font-size:11px;cursor:pointer;background:#0f1117;color:#64748b;transition:.15s}
.mode-btn.active{background:#3b4480;border-color:#7c83ff;color:#c7d2fe}
.ctrl-btn{width:100%;padding:7px 0;border-radius:8px;border:none;
  font-size:13px;font-weight:600;cursor:pointer;transition:.15s}
.ctrl-btn.turn-on{background:#166534;color:#86efac}
.ctrl-btn.turn-off{background:#7f1d1d;color:#fca5a5}
.ctrl-btn:disabled{opacity:.35;cursor:not-allowed}
.footer{text-align:center;font-size:11px;color:#374151;padding:24px 0 12px}
.ip-chip{display:inline-flex;align-items:center;gap:5px;background:#1a1d2e;
  border:1px solid #2d3148;border-radius:6px;padding:4px 10px;font-size:12px;color:#64748b;margin-bottom:12px}
.dot{width:7px;height:7px;border-radius:50%;background:#22c55e;display:inline-block}
</style>
</head><body>
<header>
  <h1>🌿 Smart Greenhouse</h1>
  <span id="hdr-time">--:--:--</span>
</header>
<div class="container">

  <!-- Fault banner -->
  <div class="fault-banner" id="fault-banner">
    <div class="fault-icon">🚨</div>
    <div class="fault-text">
      <h3 id="fault-title">Fault</h3>
      <p id="fault-detail"></p>
    </div>
  </div>

  <!-- Sensor section -->
  <h2>Sensor Data</h2>
  <div class="grid">
    <div class="card">
      <div class="card-label">Temperature</div>
      <div><span class="card-value" id="s-temp">--</span><span class="card-unit">°C</span></div>
      <div class="bar-wrap"><div class="bar-fill bar-orange" id="b-temp" style="width:0%"></div></div>
      <div class="card-sub" id="s-temp-st">--</div>
    </div>
    <div class="card">
      <div class="card-label">Humidity</div>
      <div><span class="card-value" id="s-hum">--</span><span class="card-unit">%</span></div>
      <div class="bar-wrap"><div class="bar-fill bar-blue" id="b-hum" style="width:0%"></div></div>
      <div class="card-sub" id="s-hum-st">Mist: <span id="s-mist-st">--</span></div>
    </div>
    <div class="card">
      <div class="card-label">Soil Zone A</div>
      <div><span class="card-value" id="s-soilA">--</span><span class="card-unit">%</span></div>
      <div class="bar-wrap"><div class="bar-fill bar-green" id="b-soilA" style="width:0%"></div></div>
      <div class="card-sub" id="s-soilA-st">--</div>
    </div>
    <div class="card">
      <div class="card-label">Soil Zone B</div>
      <div><span class="card-value" id="s-soilB">--</span><span class="card-unit">%</span></div>
      <div class="bar-wrap"><div class="bar-fill bar-green" id="b-soilB" style="width:0%"></div></div>
      <div class="card-sub" id="s-soilB-st">--</div>
    </div>
    <div class="card">
      <div class="card-label">Gas Level</div>
      <div><span class="card-value" id="s-gas">--</span><span class="card-unit">raw</span></div>
      <div class="bar-wrap"><div class="bar-fill bar-red" id="b-gas" style="width:0%"></div></div>
      <div class="card-sub" id="s-gas-st">--</div>
    </div>
    <div class="card">
      <div class="card-label">Light</div>
      <div><span class="card-value" id="s-lux">--</span><span class="card-unit">raw</span></div>
      <div class="bar-wrap"><div class="bar-fill bar-orange" id="b-lux" style="width:0%"></div></div>
      <div class="card-sub" id="s-lux-st">--</div>
    </div>
  </div>

  <!-- Actuator section -->
  <h2>Actuator Control</h2>
  <div class="act-grid" id="act-grid">
    <!-- injected by JS -->
  </div>

</div>
<div class="footer">Smart Greenhouse ESP32 &nbsp;|&nbsp; <span id="ip-label"></span></div>

<script>
const ACTS = [
  {id:'pumpA', label:'Pump A', zone:'Zone A'},
  {id:'pumpB', label:'Pump B', zone:'Zone B'},
  {id:'fan',   label:'Fan',    zone:'Climate'},
  {id:'mist',  label:'Mist',   zone:'Humidity'},
  {id:'shade', label:'Shade',  zone:'Light'},
];

// Build actuator cards once
function buildActCards() {
  const g = document.getElementById('act-grid');
  g.innerHTML = ACTS.map(a => `
    <div class="act-card" id="ac-${a.id}">
      <div class="act-name">${a.label} <small style="color:#374151">${a.zone}</small></div>
      <div class="act-state off" id="as-${a.id}">OFF</div>
      <div class="mode-row">
        <button class="mode-btn active" id="mb-auto-${a.id}" onclick="setMode('${a.id}','auto')">AUTO</button>
        <button class="mode-btn"       id="mb-man-${a.id}"  onclick="setMode('${a.id}','manual')">MANUAL</button>
      </div>
      <button class="ctrl-btn turn-on" id="cb-${a.id}" onclick="toggleAct('${a.id}')" disabled>Turn ON</button>
    </div>`).join('');
}
buildActCards();

let actStates = {};
let actModes  = {};

function setMode(id, mode) {
  fetch('/api/mode', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({actuator: id, mode: mode})
  }).then(() => refreshData());
}

function toggleAct(id) {
  const isOn = actStates[id];
  fetch('/api/control', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({actuator: id, state: !isOn})
  }).then(() => refreshData());
}

function pct(val, max) { return Math.min(100, Math.max(0, val/max*100)).toFixed(1); }

function refreshData() {
  fetch('/api/data')
    .then(r => r.json())
    .then(d => {
      // Time
      document.getElementById('hdr-time').textContent = new Date().toLocaleTimeString();

      // Sensors
      const tFault = d.dhtFault;
      document.getElementById('s-temp').textContent = tFault ? '--' : d.temp.toFixed(1);
      document.getElementById('s-hum').textContent  = tFault ? '--' : d.hum.toFixed(1);
      document.getElementById('b-temp').style.width = tFault ? '0%' : pct(d.temp,50)+'%';
      document.getElementById('b-hum').style.width  = tFault ? '0%' : d.hum+'%';

      const tSt = tFault ? '<span class="status-err">FAULT</span>' :
                  d.temp > 35 ? '<span class="status-warn">High</span>' : '<span class="status-ok">OK</span>';
      document.getElementById('s-temp-st').innerHTML = tSt;
      document.getElementById('s-mist-st').textContent = d.mistState;

      // Soil A
      const aFault = d.soilA_fault;
      document.getElementById('s-soilA').textContent = aFault ? '--' : d.soilA.toFixed(0);
      document.getElementById('b-soilA').style.width = aFault ? '0%' : d.soilA+'%';
      document.getElementById('s-soilA-st').innerHTML = aFault
        ? '<span class="status-err">DISCONNECTED</span>'
        : d.soilA < 35 ? '<span class="status-warn">Dry — pump running</span>'
        : d.soilA > 70 ? '<span class="status-ok">Wet — pump off</span>'
        : 'Normal';

      // Soil B
      const bFault = d.soilB_fault;
      document.getElementById('s-soilB').textContent = bFault ? '--' : d.soilB.toFixed(0);
      document.getElementById('b-soilB').style.width = bFault ? '0%' : d.soilB+'%';
      document.getElementById('s-soilB-st').innerHTML = bFault
        ? '<span class="status-err">DISCONNECTED</span>'
        : d.soilB < 35 ? '<span class="status-warn">Dry — pump running</span>'
        : d.soilB > 70 ? '<span class="status-ok">Wet — pump off</span>'
        : 'Normal';

      // Gas
      document.getElementById('s-gas').textContent = d.gas;
      document.getElementById('b-gas').style.width = pct(d.gas,4095)+'%';
      document.getElementById('s-gas-st').innerHTML = d.gas > 1500
        ? '<span class="status-err">DANGER</span>' : '<span class="status-ok">Safe</span>';

      // Light
      document.getElementById('s-lux').textContent = d.light;
      document.getElementById('b-lux').style.width = pct(d.light,4095)+'%';
      document.getElementById('s-lux-st').innerHTML = d.light > 3000
        ? '<span class="status-warn">High — shade deployed</span>' : 'Normal';

      // Fault banner
      const banner = document.getElementById('fault-banner');
      if (d.faultCode > 0) {
        banner.classList.add('active');
        document.getElementById('fault-title').textContent = 'Fault Code ' + d.faultCode;
        document.getElementById('fault-detail').textContent = d.faultDetail;
      } else {
        banner.classList.remove('active');
      }

      // Actuator cards
      actStates = d.actuators;
      actModes  = d.modes;
      ACTS.forEach(a => {
        const isOn  = d.actuators[a.id];
        const isMan = d.modes[a.id] === 'manual';
        const stEl  = document.getElementById('as-' + a.id);
        const cbEl  = document.getElementById('cb-' + a.id);
        const maEl  = document.getElementById('mb-auto-' + a.id);
        const mmEl  = document.getElementById('mb-man-' + a.id);

        stEl.textContent = isOn ? 'ON' : 'OFF';
        stEl.className   = 'act-state ' + (isOn ? 'on' : 'off');

        maEl.classList.toggle('active', !isMan);
        mmEl.classList.toggle('active',  isMan);

        cbEl.disabled = !isMan;
        cbEl.textContent   = isOn ? 'Turn OFF' : 'Turn ON';
        cbEl.className = isOn
          ? 'ctrl-btn turn-off'
          : 'ctrl-btn turn-on';
      });

      document.getElementById('ip-label').textContent = d.ip;
    })
    .catch(() => {
      document.getElementById('hdr-time').textContent = 'Offline';
    });
}

refreshData();
setInterval(refreshData, 2000);
</script>
</body></html>
)rawhtml";

// =================================================================

bool WebServerManager::connectWiFi() {
#if !WIFI_ENABLED
  Serial.println(F("[WiFi] Disabled in config."));
  return false;
#endif
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 15000) {
      Serial.println(F("\n[WiFi] Timeout."));
      return false;
    }
    delay(500); Serial.print(".");
  }
  Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void WebServerManager::begin(ActuatorManager* act) {
#if !WIFI_ENABLED
  return;
#endif
  _act = act;
  _setupRoutes();
  _server.begin();
  Serial.printf("[Web] Server started on port %d\n", WEB_PORT);
}

void WebServerManager::update(const SensorData& d,
                               const FaultEvent& fault,
                               const MistController& mist) {
  _lastData  = d;
  _lastFault = fault;
  _mistState = mist.stateName();
}

// =================================================================
//  Routes
// =================================================================
void WebServerManager::_setupRoutes() {

  // ── GET / → serve dashboard HTML ─────────────────────────
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", HTML_PAGE);
  });

  // ── GET /api/data → JSON sensor + actuator snapshot ──────
  _server.on("/api/data", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", _buildJSON());
  });

  // ── POST /api/mode → set actuator to AUTO or MANUAL ──────
  _server.on("/api/mode", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
      }
      String actuator = doc["actuator"].as<String>();
      String mode     = doc["mode"].as<String>();
      CtrlMode cm = (mode == "manual") ? CtrlMode::MANUAL : CtrlMode::AUTO;
      if (_act) _act->setMode(actuator, cm);
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // ── POST /api/control → manual ON/OFF ────────────────────
  _server.on("/api/control", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
      }
      String act   = doc["actuator"].as<String>();
      bool   state = doc["state"].as<bool>();
      if (_act) {
        if      (act == "pumpA") _act->setManualPumpA(state);
        else if (act == "pumpB") _act->setManualPumpB(state);
        else if (act == "fan")   _act->setManualFan(state);
        else if (act == "mist")  _act->setManualMist(state);
        else if (act == "shade") _act->setManualShade(state);
      }
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // ── 404 ───────────────────────────────────────────────────
  _server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "Not found");
  });
}

// =================================================================
//  Build JSON snapshot
// =================================================================
String WebServerManager::_buildJSON() {
  JsonDocument doc;

  // Sensors
  doc["temp"]       = _lastData.dhtFault ? 0     : _lastData.temperature;
  doc["hum"]        = _lastData.dhtFault ? 0     : _lastData.humidity;
  doc["soilA"]      = _lastData.soilA_fault ? 0  : _lastData.soilA_pct;
  doc["soilB"]      = _lastData.soilB_fault ? 0  : _lastData.soilB_pct;
  doc["gas"]        = _lastData.gasLevel;
  doc["light"]      = _lastData.lightLevel;
  doc["dhtFault"]    = _lastData.dhtFault;      // confirmed 2-min fault
  doc["dhtGlitch"]   = _lastData.dhtRawFail;    // short glitch (not a real fault)
  doc["soilA_fault"]= _lastData.soilA_fault;
  doc["soilB_fault"]= _lastData.soilB_fault;
  doc["mistState"]  = _mistState;

  // Fault
  doc["faultCode"]   = (int)_lastFault.code;
  doc["faultDetail"] = _lastFault.detail;

  // Actuators state
  JsonObject act = doc["actuators"].to<JsonObject>();
  if (_act) {
    act["pumpA"] = _act->isPumpAOn();
    act["pumpB"] = _act->isPumpBOn();
    act["fan"]   = _act->isFanOn();
    act["mist"]  = _act->isMistOn();
    act["shade"] = !_act->isShadeOpen();
  }

  // Modes
  JsonObject modes = doc["modes"].to<JsonObject>();
  if (_act) {
    modes["pumpA"] = _act->getMode("pumpA")==CtrlMode::MANUAL?"manual":"auto";
    modes["pumpB"] = _act->getMode("pumpB")==CtrlMode::MANUAL?"manual":"auto";
    modes["fan"]   = _act->getMode("fan")  ==CtrlMode::MANUAL?"manual":"auto";
    modes["mist"]  = _act->getMode("mist") ==CtrlMode::MANUAL?"manual":"auto";
    modes["shade"] = _act->getMode("shade")==CtrlMode::MANUAL?"manual":"auto";
  }

  doc["ip"] = WiFi.localIP().toString();

  String out;
  serializeJson(doc, out);
  return out;
}
