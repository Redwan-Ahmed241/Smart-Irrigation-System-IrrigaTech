// =================================================================
//  sim800l_manager.cpp  —  WATCHDOG FIXED
//
//  Changes from previous version:
//    1. esp_task_wdt_reset() called inside every _waitFor() loop
//    2. _doSend() prompt timeout: 8000 → 5000 ms
//    3. _doSend() send timeout: 30000 → 15000 ms
//    4. _handshake() delays feed watchdog
//    5. begin() boot delay feeds watchdog in 500ms chunks
//    6. _atQuery() feeds watchdog
//
//  ESP32 TX(17) → SIM800L RX  |  ESP32 RX(16) ← SIM800L TX
//  SIM800L power: 3.7–4.2V, 2A peak — use separate supply!
// =================================================================
#include "sim800l_manager.h"
#include "esp_task_wdt.h"

#define SIM Serial2

// ── begin ─────────────────────────────────────────────────────
void SIM800LManager::begin() {
#if !SIM800L_ENABLED
  Serial.println(F("[SIM800L] Disabled in config."));
  return;
#endif
  SIM.begin(SIM_BAUD, SERIAL_8N1, PIN_SIM_RX, PIN_SIM_TX);

  // Boot delay in 500ms chunks so watchdog stays fed
  Serial.println(F("[SIM800L] Waiting for module boot..."));
  for (int i = 0; i < 6; i++) {   // 6 × 500ms = 3 s
    delay(500);
    esp_task_wdt_reset();
  }

  _flush();

  // Quick probe — already up? (warm reboot)
  SIM.println("AT");
  if (_waitFor("OK", 500)) {
    Serial.println(F("[SIM800L] Already responsive (warm boot)."));
  }

  _flush();
  _ready = _handshake();
  Serial.printf("[SIM800L] %s\n",
    _ready ? "Ready." : "No response — check power/wiring/baud.");
}

// ── update — executes queued SMS non-blocking ─────────────────
void SIM800LManager::update() {
#if !SIM800L_ENABLED
  return;
#endif
  if (!_hasPending || _busy) return;
  _hasPending = false;
  _busy       = true;
  bool ok = _doSend(_pendNum, _pendMsg);
  _busy = false;
  if (!ok) {
    Serial.println(F("[SIM800L] SMS send failed."));
    // Re-check module health after a failed send
    _ready = _atCmd("AT", "OK", 1000);
  }
}

// ── sendSMS — queues message, returns immediately ─────────────
bool SIM800LManager::sendSMS(const String& number, const String& msg) {
#if !SIM800L_ENABLED
  Serial.printf("[SIM800L] DISABLED — SMS not sent: %s\n", msg.c_str());
  return false;
#endif
  if (!_ready) {
    Serial.println(F("[SIM800L] Not ready — SMS dropped."));
    return false;
  }
  if (_hasPending || _busy) {
    Serial.println(F("[SIM800L] Busy — SMS dropped."));
    return false;
  }
  _pendNum    = number;
  _pendMsg    = msg;
  _hasPending = true;
  Serial.printf("[SIM800L] SMS queued to %s\n", number.c_str());
  return true;
}

// ── signalBars — 0 to 5 ──────────────────────────────────────
int SIM800LManager::signalBars() {
#if !SIM800L_ENABLED
  return 0;
#endif
  String r = _atQuery("AT+CSQ", 2000);
  int idx = r.indexOf("+CSQ:");
  if (idx == -1) return 0;
  int rssi = r.substring(idx + 5).toInt();
  if (rssi == 99 || rssi < 0) return 0;
  if (rssi < 4)  return 1;
  if (rssi < 9)  return 2;
  if (rssi < 15) return 3;
  if (rssi < 21) return 4;
  return 5;
}

// ── Private: AT handshake sequence ───────────────────────────
bool SIM800LManager::_handshake() {
  // Try AT up to 5 times with watchdog feeds between attempts
  for (int i = 0; i < 5; i++) {
    if (_atCmd("AT", "OK", 1000)) break;
    if (i == 4) return false;
    esp_task_wdt_reset();
    delay(500);
    esp_task_wdt_reset();
  }
  _atCmd("ATE0",            "OK", 1000);  // Echo off
  _atCmd("AT+CMGF=1",       "OK", 1000);  // SMS text mode
  _atCmd("AT+CSCS=\"GSM\"", "OK", 1000);  // GSM charset
  return true;
}

// ── Private: execute SMS send — FIXED timeouts ───────────────
bool SIM800LManager::_doSend(const String& num, const String& msg) {
  Serial.printf("[SIM800L] Sending to %s...\n", num.c_str());

  esp_task_wdt_reset();

  // Step 1: send AT+CMGS command
  String cmd = "AT+CMGS=\"" + num + "\"";
  SIM.println(cmd);

  // Step 2: wait for '>' prompt — REDUCED from 8000 to 5000 ms
  if (!_waitFor(">", 5000)) {
    Serial.println(F("[SIM800L] No '>' prompt received."));
    esp_task_wdt_reset();
    return false;
  }

  esp_task_wdt_reset();

  // Step 3: send message + Ctrl+Z
  SIM.print(msg);
  delay(100);
  esp_task_wdt_reset();
  SIM.write(0x1A);

  // Step 4: wait for +CMGS confirmation — REDUCED from 30000 to 15000 ms
  // _waitFor feeds watchdog internally every iteration
  bool ok = _waitFor("+CMGS:", 15000);

  esp_task_wdt_reset();
  Serial.println(ok ? F("[SIM800L] Sent OK.") : F("[SIM800L] Send FAILED."));
  return ok;
}

// ── Private: send AT command, wait for expected response ─────
bool SIM800LManager::_atCmd(const String& cmd, const String& expect,
                              unsigned long timeout) {
  _flush();
  SIM.println(cmd);
  return _waitFor(expect, timeout);
}

// ── Private: send AT command, return full response string ────
String SIM800LManager::_atQuery(const String& cmd, unsigned long timeout) {
  _flush();
  SIM.println(cmd);
  unsigned long start = millis();
  String buf = "";
  while (millis() - start < timeout) {
    while (SIM.available()) buf += (char)SIM.read();
    if (buf.indexOf("OK")    != -1) break;
    if (buf.indexOf("ERROR") != -1) break;
    esp_task_wdt_reset();   // ← feed watchdog
    delay(10);
  }
  esp_task_wdt_reset();
  return buf;
}

// ── Private: wait for token in serial stream — WATCHDOG FIXED
bool SIM800LManager::_waitFor(const String& token, unsigned long timeout) {
  unsigned long start = millis();
  String buf = "";
  while (millis() - start < timeout) {
    while (SIM.available()) {
      buf += (char)SIM.read();
      if (buf.indexOf(token) != -1) {
        esp_task_wdt_reset();
        return true;
      }
      if (buf.length() > 256) buf = buf.substring(128);
    }
    esp_task_wdt_reset();   // ← feed watchdog EVERY iteration
    delay(10);
  }
  esp_task_wdt_reset();
  return false;
}

// ── Private: discard any buffered serial data ────────────────
void SIM800LManager::_flush() {
  esp_task_wdt_reset();
  delay(20);
  while (SIM.available()) SIM.read();
  esp_task_wdt_reset();
}
