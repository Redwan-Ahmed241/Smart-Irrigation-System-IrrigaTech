// =================================================================
//  main.cpp  —  Smart Greenhouse v2  —  WATCHDOG FIXED
//
//  Hardware:
//    2x Soil sensor (analog)   → GPIO 34 (A), 35 (B)
//    1x DHT22                  → GPIO 4
//    1x LDR                    → GPIO 32  (HIGH=dark, LOW=bright)
//    1x MQ-2 gas sensor        → GPIO 33
//    2x Water pump relay       → GPIO 26 (A), 27 (B)
//    1x Fan relay              → GPIO 14
//    1x Ultrasonic mist maker  → GPIO 12  (LOW=ON, HIGH=OFF)
//    1x Servo (shade)          → GPIO 13
//    1x Active buzzer          → GPIO 25
//    SSD1306 OLED 128x64       → SDA 21, SCL 22
//    SIM800L GSM               → TX 17, RX 16
//
//  Feature flags (config.h):
//    FAULT_DETECTION_ENABLED   1/0
//    BUZZER_ENABLED            1/0
//    WIFI_ENABLED              1/0
//    SIM800L_ENABLED           1/0
//    OLED_ENABLED              1/0
//
//  Watchdog:
//    esp_task_wdt configured to 15 s timeout.
//    Fed at end of every loop tick.
//    Also fed inside sim800l blocking calls.
// =================================================================

#include <Arduino.h>
#include "esp_task_wdt.h"       // ← Watchdog header
#include "config.h"
#include "sensor_manager.h"
#include "logic_controller.h"
#include "mist_controller.h"
#include "fault_monitor.h"
#include "actuator_manager.h"
#include "display_manager.h"
#include "sim800l_manager.h"
#include "alert_manager.h"
#include "web_server_manager.h"

// ── Watchdog timeout ──────────────────────────────────────────
// Must be longer than the longest single blocking operation.
// SIM800L send can take up to ~16 s (5s prompt + 15s confirm) with feeds.
// Set to 20 s to give plenty of margin.
#define WDT_TIMEOUT_SEC   20

// ── Module instances ──────────────────────────────────────────
SensorManager    sensors;
LogicController  logic;
MistController   mistCtrl;
FaultMonitor     faultMon;
ActuatorManager  actuators;
DisplayManager   display;
SIM800LManager   sim800l;
AlertManager     alerts;
WebServerManager webServer;

// ── System state ──────────────────────────────────────────────
SensorData       sData;
ActuatorCommands cmd     = {false, false, false, false, true};
FaultEvent       fault;

// Pump ON-time trackers (for stuck-pump fault detection)
unsigned long pumpAonAt = 0;
unsigned long pumpBonAt = 0;

// Interval timers
unsigned long lastSensorRead = 0;
unsigned long lastDisplay    = 0;
unsigned long lastWebUpdate  = 0;
unsigned long lastDebugPrint = 0;

// =================================================================
void setup() {

   esp_reset_reason_t reason = esp_reset_reason();
  switch(reason) {
    case ESP_RST_POWERON:   Serial.println(F("Reset: Power ON"));        break;
    case ESP_RST_BROWNOUT:  Serial.println(F("Reset: BROWNOUT — CHECK POWER SUPPLY!")); break;
    case ESP_RST_PANIC:     Serial.println(F("Reset: Software PANIC/crash")); break;
    case ESP_RST_WDT:       Serial.println(F("Reset: Watchdog timeout")); break;
    case ESP_RST_SW:        Serial.println(F("Reset: Software reset"));   break;
    case ESP_RST_DEEPSLEEP: Serial.println(F("Reset: Wake from sleep"));  break;
    default:                Serial.printf("Reset: reason code %d\n", reason); break;
  }
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== Smart Greenhouse v2 — Boot ==="));

  // ── Watchdog: initialise BEFORE any slow begin() calls ───────
  // true = panic (reset) on timeout
  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
  esp_task_wdt_add(NULL);   // watch the main/loop task
  Serial.printf("[WDT] Watchdog armed — %d s timeout\n", WDT_TIMEOUT_SEC);

  // ── 1. Actuators first — safe state before power comes up ─────
  actuators.begin();
  esp_task_wdt_reset();

  // ── 2. OLED ──────────────────────────────────────────────────
  display.begin();
  display.showBoot();
  esp_task_wdt_reset();

  // ── 3. Sensors ───────────────────────────────────────────────
  sensors.begin();     // DHT22 has 2s internal delay — that's fine
  esp_task_wdt_reset();

  // ── 4. SIM800L (slow — 3s boot, watchdog fed inside begin()) ─
  sim800l.begin();
  esp_task_wdt_reset();

  // ── 5. Alerts ────────────────────────────────────────────────
  alerts.begin(&sim800l, &actuators);
  esp_task_wdt_reset();

  // ── 6. WiFi + web server ──────────────────────────────────────
#if WIFI_ENABLED
  bool wifiOk = webServer.connectWiFi();
  esp_task_wdt_reset();
  if (wifiOk) {
    webServer.begin(&actuators);
    Serial.printf("[Main] Web dashboard: http://%s/\n",
                  webServer.localIP().c_str());
  }
  esp_task_wdt_reset();
#endif

  // ── 7. Logic modules ─────────────────────────────────────────
  mistCtrl.begin();
  faultMon.begin();
  esp_task_wdt_reset();

  // ── 8. First sensor read ──────────────────────────────────────
  sData = sensors.readAll();
  sensors.printSerial(sData);
  esp_task_wdt_reset();

  Serial.println(F("=== Boot complete. Entering loop. ===\n"));
}

// =================================================================
void loop() {
  unsigned long now = millis();

  // ══ 1. READ SENSORS (every 2 s) ══════════════════════════════
  if (now - lastSensorRead >= INTERVAL_SENSOR) {
    sData          = sensors.readAll();
    lastSensorRead = now;
  }

  // ══ 2. DECISION LOGIC ═════════════════════════════════════════
  cmd = logic.evaluate(sData, cmd);

  // ══ 3. MIST STATE MACHINE ═════════════════════════════════════
  cmd.mist = mistCtrl.update(sData);

  // ══ 4. FAULT DETECTION ════════════════════════════════════════
  unsigned long pumpAms = (actuators.isPumpAOn() && pumpAonAt > 0)
                          ? (now - pumpAonAt) : 0;
  unsigned long pumpBms = (actuators.isPumpBOn() && pumpBonAt > 0)
                          ? (now - pumpBonAt) : 0;
  unsigned long mistMs  = mistCtrl.onDurationMs();

  fault = faultMon.check(
    sData,
    actuators.isPumpAOn(), pumpAms,
    actuators.isPumpBOn(), pumpBms,
    actuators.isMistOn(),  mistMs
  );

  // ══ 5. CRITICAL FAULT OVERRIDE ════════════════════════════════
  // Gas critical: cut pumps + mist, keep fan ON
  if (fault.isCritical) {
    actuators.emergencyStop();
    cmd = {false, false, true, false, true};
  }

  // ══ 6. APPLY ACTUATORS ════════════════════════════════════════
  actuators.setPumpA(cmd.pumpA);
  actuators.setPumpB(cmd.pumpB);
  actuators.setFan(cmd.fan);
  actuators.setMist(cmd.mist);
  actuators.setShade(cmd.shadeOpen);

  // Track pump ON start times
  if (cmd.pumpA && pumpAonAt == 0) pumpAonAt = now;
  if (!cmd.pumpA)                   pumpAonAt = 0;
  if (cmd.pumpB && pumpBonAt == 0) pumpBonAt = now;
  if (!cmd.pumpB)                   pumpBonAt = 0;

  // ══ 7. ALERTS (SMS + buzzer scheduling) ═══════════════════════
  alerts.handle(fault);
  alerts.checkHeartbeat(
    sim800l.signalBars(),
    sData.temperature, sData.humidity,
    sData.soilA_pct,   sData.soilB_pct
  );

  // ══ 8. BUZZER NON-BLOCKING UPDATE ═════════════════════════════
  actuators.buzzerUpdate();

  // ══ 9. OLED DISPLAY ═══════════════════════════════════════════
  if (now - lastDisplay >= INTERVAL_DISPLAY) {
    display.update(sData, actuators, mistCtrl, fault,
                   sim800l.signalBars(), webServer.isConnected());
    lastDisplay = now;
  }

  // ══ 10. DEBUG PRINT (every 10 s) ══════════════════════════════
  if (now - lastDebugPrint >= 10000UL) {
    printSystemState(sData, cmd, fault);
    lastDebugPrint = now;
  }

  // ══ 11. WEB SERVER DATA UPDATE ════════════════════════════════
#if WIFI_ENABLED
  if (now - lastWebUpdate >= INTERVAL_WEB) {
    webServer.update(sData, fault, mistCtrl);
    lastWebUpdate = now;
  }
#endif

  // ══ 12. SIM800L NON-BLOCKING EXECUTE ══════════════════════════
  // update() calls _doSend() which feeds watchdog internally
  sim800l.update();

  // ══ 13. FEED WATCHDOG + LOOP TICK ═════════════════════════════
  esp_task_wdt_reset();   // ← Must be called every loop iteration
  delay(50);
}
