// =================================================================
//  display_manager.cpp
// =================================================================
#include "display_manager.h"

void DisplayManager::begin() {
#if !OLED_ENABLED
  return;
#endif
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[Display] OLED FAILED — check I2C wiring."));
    return;
  }
  _oled.setTextColor(SSD1306_WHITE);
  _oled.clearDisplay();
  _oled.display();
  _pageTimer = millis();
  Serial.println(F("[Display] OLED OK."));
}

void DisplayManager::showBoot() {
#if !OLED_ENABLED
  return;
#endif
  _clear();
  _oled.setTextSize(1);
  _oled.setCursor(10, 4);   _oled.print(F("Smart Greenhouse v2"));
  _oled.drawLine(0, 14, 127, 14, SSD1306_WHITE);
  _oled.setCursor(2, 18);   _oled.print(F("2-Zone Irrigation"));
  _oled.setCursor(2, 30);   _oled.print(F("DHT22 + 2xSoil"));
  _oled.setCursor(2, 42);   _oled.print(F("MQ2 + LDR + Mist"));
  _oled.setCursor(2, 54);   _oled.print(F("Initializing..."));
  _show();
}

void DisplayManager::update(const SensorData& d,
                              const ActuatorManager& act,
                              const MistController& mist,
                              const FaultEvent& fault,
                              int simBars, bool wifiOk) {
#if !OLED_ENABLED
  return;
#endif
  _clear();

  // Fault screen overrides everything
  if (fault.code != FaultCode::NONE) {
    _pageFault(fault);
    _show();
    return;
  }

  // Rotate pages
  if (millis() - _pageTimer >= PAGE_MS) {
    _page = (_page + 1) % 4;
    _pageTimer = millis();
  }

  switch (_page) {
    case 0: _pageClimate(d, mist);                          break;
    case 1: _pageSoil(d, act.isPumpAOn(), act.isPumpBOn()); break;
    case 2: _pageActuators(act);                            break;
    case 3: _pageEnv(d);                                    break;
  }

  _statusBar(simBars, wifiOk);
  _show();
}

// ── Page 0: Climate ───────────────────────────────────────────
void DisplayManager::_pageClimate(const SensorData& d, const MistController& mist) {
  _header("CLIMATE");
  _oled.setCursor(0, 14);
  if (d.dhtFault) {
    _oled.print(F("Temp: FAULT(conf)"));
    _oled.setCursor(0, 26);
    _oled.print(F("Hum : FAULT(conf)"));
  } else if (d.dhtRawFail) {
    _oled.print(F("Temp: glitch..."));
    _oled.setCursor(0, 26);
    _oled.print(F("Hum : glitch..."));
  } else {
    _oled.printf("Temp: %.1f C", d.temperature);
    _oled.setCursor(0, 26);
    _oled.printf("Hum : %.1f%%  [%s]", d.humidity, mist.stateName().c_str());
  }
  // Humidity progress bar
  if (!d.dhtFault) {
    int bw = (int)map((long)d.humidity, 0, 100, 0, 76);
    _oled.drawRect(50, 36, 78, 8, SSD1306_WHITE);
    _oled.fillRect(51, 37, bw, 6, SSD1306_WHITE);
    _oled.setCursor(0, 38);
    _oled.print(F("Hum%"));
  }
}

// ── Page 1: Soil ──────────────────────────────────────────────
void DisplayManager::_pageSoil(const SensorData& d, bool pumpA, bool pumpB) {
  _header("SOIL MOISTURE");
  // Zone A
  _oled.setCursor(0, 14);
  if (d.soilA_fault) {
    _oled.print(F("A: DISCONNECTED"));
  } else {
    _oled.printf("A:%3.0f%%", d.soilA_pct);
    int bw = (int)map((long)d.soilA_pct, 0, 100, 0, 52);
    _oled.drawRect(40, 14, 54, 8, SSD1306_WHITE);
    _oled.fillRect(41, 15, bw, 6, SSD1306_WHITE);
    _oled.setCursor(96, 14);
    _oled.print(pumpA ? F("PMP!") : F("    "));
  }
  // Zone B
  _oled.setCursor(0, 28);
  if (d.soilB_fault) {
    _oled.print(F("B: DISCONNECTED"));
  } else {
    _oled.printf("B:%3.0f%%", d.soilB_pct);
    int bw = (int)map((long)d.soilB_pct, 0, 100, 0, 52);
    _oled.drawRect(40, 28, 54, 8, SSD1306_WHITE);
    _oled.fillRect(41, 29, bw, 6, SSD1306_WHITE);
    _oled.setCursor(96, 28);
    _oled.print(pumpB ? F("PMP!") : F("    "));
  }
  _oled.setCursor(0, 42);
  _oled.printf("Dry<%d%% Wet>%d%%", SOIL_DRY_PCT, SOIL_WET_PCT);
}

// ── Page 2: Actuators ─────────────────────────────────────────
void DisplayManager::_pageActuators(const ActuatorManager& act) {
  _header("ACTUATORS");
  _oled.setCursor(0, 14);
  _oled.printf("PmpA:%-3s PmpB:%-3s",
    act.isPumpAOn()?"ON":"OFF", act.isPumpBOn()?"ON":"OFF");
  _oled.setCursor(0, 26);
  _oled.printf("Fan :%-3s Mist:%-3s",
    act.isFanOn() ?"ON":"OFF", act.isMistOn() ?"ON":"OFF");
  _oled.setCursor(0, 38);
  _oled.printf("Shade: %s", act.isShadeOpen()?"OPEN  ":"CLOSED");
  _oled.setCursor(0, 50);
  // Show manual overrides
  bool anyManual = (act.getMode("pumpA")==CtrlMode::MANUAL ||
                    act.getMode("pumpB")==CtrlMode::MANUAL ||
                    act.getMode("fan")  ==CtrlMode::MANUAL ||
                    act.getMode("mist") ==CtrlMode::MANUAL);
  if (anyManual) _oled.print(F("[MANUAL OVERRIDE]"));
}

// ── Page 3: Environment ───────────────────────────────────────
void DisplayManager::_pageEnv(const SensorData& d) {
  _header("ENVIRONMENT");
  // Gas
  _oled.setCursor(0, 14);
  _oled.printf("Gas :%4d", d.gasLevel);
  int gw = (int)map(constrain(d.gasLevel,0,4095),0,4095,0,48);
  _oled.drawRect(58, 14, 50, 8, SSD1306_WHITE);
  _oled.fillRect(59, 15, gw, 6, SSD1306_WHITE);
  _oled.setCursor(110, 14);
  _oled.print(d.gasLevel > GAS_DANGER ? F("!!") : F("OK"));
  // Light — HIGH=dark, LOW=bright for this LDR
  _oled.setCursor(0, 28);
  _oled.printf("Lux :%4d", d.lightLevel);
  int lw = (int)map(constrain(d.lightLevel,0,4095),0,4095,0,48);
  _oled.drawRect(58, 28, 50, 8, SSD1306_WHITE);
  _oled.fillRect(59, 29, lw, 6, SSD1306_WHITE);
  _oled.setCursor(110, 28);
  // LOW value = bright light → shade deploys
  _oled.print(d.lightLevel < LIGHT_DEPLOY_SHADE ? F("BR") : F("DK"));
  _oled.setCursor(0, 42);
  _oled.printf("Gas limit: %d", GAS_DANGER);
}

// ── Fault screen ──────────────────────────────────────────────
void DisplayManager::_pageFault(const FaultEvent& ev) {
  // Flashing border every 400ms
  if ((millis() / 400) % 2 == 0)
    _oled.drawRect(0, 0, OLED_W, OLED_H, SSD1306_WHITE);

  // Title
  _oled.fillRect(0, 0, OLED_W, 12, SSD1306_WHITE);
  _oled.setTextColor(SSD1306_BLACK);
  _oled.setCursor(2, 2);
  _oled.print(F("!! FAULT !!"));
  _oled.setTextColor(SSD1306_WHITE);

  // Detail (up to 3 lines of 20 chars)
  String s = ev.detail;
  _oled.setCursor(2, 15);
  _oled.print(s.substring(0, 20));
  if (s.length() > 20) { _oled.setCursor(2, 27); _oled.print(s.substring(20, 40)); }
  if (s.length() > 40) { _oled.setCursor(2, 39); _oled.print(s.substring(40, 60)); }

  // Bottom: SMS status
  _oled.setCursor(2, 52);
  _oled.print(ev.smsSent ? F("SMS sent to owner") : F("Sending SMS..."));
}

// ── Bottom status bar ─────────────────────────────────────────
void DisplayManager::_statusBar(int simBars, bool wifiOk) {
  _oled.drawLine(0, 54, 127, 54, SSD1306_WHITE);
  _oled.setCursor(0, 56);
  _oled.print(wifiOk ? F("W:OK ") : F("W:-- "));
  _oled.print(F("G:"));
  for (int i = 0; i < 5; i++)
    _oled.print(i < simBars ? F("*") : F("."));
}

void DisplayManager::_header(const char* t) {
  _oled.fillRect(0, 0, OLED_W, 11, SSD1306_WHITE);
  _oled.setTextColor(SSD1306_BLACK);
  _oled.setCursor(2, 2);
  _oled.print(t);
  _oled.setTextColor(SSD1306_WHITE);
}

void DisplayManager::_clear() {
  _oled.clearDisplay();
  _oled.setTextSize(1);
  _oled.setTextColor(SSD1306_WHITE);
}

void DisplayManager::_show() { _oled.display(); }
