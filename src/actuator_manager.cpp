// =================================================================
//  actuator_manager.cpp
// =================================================================
#include "actuator_manager.h"

void ActuatorManager::begin() {
  int relays[] = {PIN_PUMP_A, PIN_PUMP_B, PIN_FAN};
  for (int pin : relays) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, RELAY_OFF);
  }
  // Mist maker: inverted logic — HIGH = OFF at boot
  pinMode(PIN_MIST, OUTPUT);
  digitalWrite(PIN_MIST, MIST_PIN_OFF);

#if BUZZER_ENABLED
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
#endif

  _servo.attach(PIN_SERVO_SHADE);
  _servo.write(SHADE_OPEN_DEG);
  _shadeOpen = true;

  Serial.println(F("[Actuator] All relays OFF, servo OPEN, buzzer ready."));
}

// ── AUTO setters ──────────────────────────────────────────────

void ActuatorManager::setPumpA(bool on) {
  if (_modePumpA != CtrlMode::AUTO) return;  // Manual override active
  if (on == _pumpA) return;
  _pumpA = on;
  _applyRelay(PIN_PUMP_A, on, "PUMP-A");
}

void ActuatorManager::setPumpB(bool on) {
  if (_modePumpB != CtrlMode::AUTO) return;
  if (on == _pumpB) return;
  _pumpB = on;
  _applyRelay(PIN_PUMP_B, on, "PUMP-B");
}

void ActuatorManager::setFan(bool on) {
  if (_modeFan != CtrlMode::AUTO) return;
  if (on == _fan) return;
  _fan = on;
  _applyRelay(PIN_FAN, on, "FAN");
}

void ActuatorManager::setMist(bool on) {
  if (_modeMist != CtrlMode::AUTO) return;
  if (on == _mist) return;
  _mist = on;
  // Mist maker uses INVERTED logic: LOW = ON, HIGH = OFF
  digitalWrite(PIN_MIST, on ? MIST_PIN_ON : MIST_PIN_OFF);
  Serial.printf("[Actuator] %-12s → %s\n", "MIST", on ? "ON" : "OFF");
}

void ActuatorManager::setShade(bool open) {
  if (_modeShade != CtrlMode::AUTO) return;
  if (open == _shadeOpen) return;
  _shadeOpen = open;
  _servo.write(open ? SHADE_OPEN_DEG : SHADE_CLOSED_DEG);
  Serial.printf("[Actuator] SHADE → %s\n", open ? "OPEN" : "CLOSED");
}

// ── Manual override setters (from web UI) ─────────────────────

void ActuatorManager::setManualPumpA(bool on) {
  _manPumpA = on; _pumpA = on;
  _applyRelay(PIN_PUMP_A, on, "PUMP-A[MAN]");
}
void ActuatorManager::setManualPumpB(bool on) {
  _manPumpB = on; _pumpB = on;
  _applyRelay(PIN_PUMP_B, on, "PUMP-B[MAN]");
}
void ActuatorManager::setManualFan(bool on) {
  _manFan = on; _fan = on;
  _applyRelay(PIN_FAN, on, "FAN[MAN]");
}
void ActuatorManager::setManualMist(bool on) {
  _manMist = on; _mist = on;
  // Mist maker uses INVERTED logic: LOW = ON, HIGH = OFF
  digitalWrite(PIN_MIST, on ? MIST_PIN_ON : MIST_PIN_OFF);
  Serial.printf("[Actuator] %-12s → %s\n", "MIST[MAN]", on ? "ON" : "OFF");
}
void ActuatorManager::setManualShade(bool open) {
  _manShade = open; _shadeOpen = open;
  _servo.write(open ? SHADE_OPEN_DEG : SHADE_CLOSED_DEG);
  Serial.printf("[Actuator] SHADE[MAN] → %s\n", open ? "OPEN" : "CLOSED");
}

void ActuatorManager::setMode(const String& a, CtrlMode mode) {
  if      (a == "pumpA") { _modePumpA = mode; if (mode==CtrlMode::AUTO){ _pumpA=false; _applyRelay(PIN_PUMP_A,false,"PUMP-A"); } }
  else if (a == "pumpB") { _modePumpB = mode; if (mode==CtrlMode::AUTO){ _pumpB=false; _applyRelay(PIN_PUMP_B,false,"PUMP-B"); } }
  else if (a == "fan")   { _modeFan   = mode; if (mode==CtrlMode::AUTO){ _fan=false;   _applyRelay(PIN_FAN,false,"FAN");     } }
  else if (a == "mist")  { _modeMist  = mode; if (mode==CtrlMode::AUTO){ _mist=false;  _applyRelay(PIN_MIST,false,"MIST");   } }
  else if (a == "shade") { _modeShade = mode; }
  Serial.printf("[Actuator] %s mode → %s\n", a.c_str(),
                mode == CtrlMode::AUTO ? "AUTO" : "MANUAL");
}

CtrlMode ActuatorManager::getMode(const String& a) const {
  if (a == "pumpA") return _modePumpA;
  if (a == "pumpB") return _modePumpB;
  if (a == "fan")   return _modeFan;
  if (a == "mist")  return _modeMist;
  if (a == "shade") return _modeShade;
  return CtrlMode::AUTO;
}

// ── Emergency ─────────────────────────────────────────────────
void ActuatorManager::emergencyStop() {
  Serial.println(F("[Actuator] *** EMERGENCY STOP ***"));
  // Force override manual modes for safety
  _modePumpA = _modePumpB = _modeMist = CtrlMode::AUTO;
  _modeFan = CtrlMode::AUTO;
  _pumpA = _pumpB = _mist = false;
  _fan = true;
  _applyRelay(PIN_PUMP_A, false, "PUMP-A");
  _applyRelay(PIN_PUMP_B, false, "PUMP-B");
  _applyRelay(PIN_MIST,   false, "MIST");
  _applyRelay(PIN_FAN,    true,  "FAN");
}

// ── Buzzer non-blocking state machine ─────────────────────────
// Pattern: 3 short beeps (150ms ON, 120ms OFF) then silent
// Repeats every BUZZER_INTERVAL_MS while active
void ActuatorManager::buzzerUpdate() {
#if BUZZER_ENABLED
  if (!_buzzerActive) return;

  unsigned long now = millis();

  if (_beepsDone >= BUZZER_BURST_COUNT) {
    // Burst done — stay silent until interval expires
    digitalWrite(PIN_BUZZER, LOW);
    _beepOn = false;
    if (now - _buzzerTimer >= BUZZER_INTERVAL_MS) {
      // Time to do another burst
      _beepsDone = 0;
      _buzzerTimer = now;
    }
    return;
  }

  if (_beepOn) {
    if (now - _buzzerTimer >= BUZZER_BEEP_MS) {
      digitalWrite(PIN_BUZZER, LOW);
      _beepOn = false;
      _beepsDone++;
      _buzzerTimer = now;
    }
  } else {
    if (now - _buzzerTimer >= BUZZER_PAUSE_MS) {
      if (_beepsDone < BUZZER_BURST_COUNT) {
        digitalWrite(PIN_BUZZER, HIGH);
        _beepOn = true;
        _buzzerTimer = now;
      }
    }
  }
#endif
}

void ActuatorManager::buzzerTriggerBurst() {
#if BUZZER_ENABLED
  if (_buzzerActive) return;  // Already running
  _buzzerActive  = true;
  _beepsDone     = 0;
  _beepOn        = false;
  _buzzerTimer   = millis() - BUZZER_PAUSE_MS;  // Start first beep immediately
  Serial.println(F("[Buzzer] Fault alert burst started."));
#endif
}

void ActuatorManager::buzzerStop() {
#if BUZZER_ENABLED
  _buzzerActive = false;
  _beepOn       = false;
  _beepsDone    = 0;
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println(F("[Buzzer] Stopped."));
#endif
}

// ── printStatus ───────────────────────────────────────────────
void ActuatorManager::printStatus() {
  Serial.println(F("===== Actuator Status ====="));
  Serial.printf("  PumpA : %-3s  [%s]\n", _pumpA?"ON":"OFF", _modePumpA==CtrlMode::AUTO?"AUTO":"MAN");
  Serial.printf("  PumpB : %-3s  [%s]\n", _pumpB?"ON":"OFF", _modePumpB==CtrlMode::AUTO?"AUTO":"MAN");
  Serial.printf("  Fan   : %-3s  [%s]\n", _fan  ?"ON":"OFF", _modeFan  ==CtrlMode::AUTO?"AUTO":"MAN");
  Serial.printf("  Mist  : %-3s  [%s]\n", _mist ?"ON":"OFF", _modeMist ==CtrlMode::AUTO?"AUTO":"MAN");
  Serial.printf("  Shade : %s  [%s]\n", _shadeOpen?"OPEN":"CLSD", _modeShade==CtrlMode::AUTO?"AUTO":"MAN");
  Serial.println(F("==========================="));
}

// ── Private relay helper ──────────────────────────────────────
void ActuatorManager::_applyRelay(int pin, bool on, const char* label) {
  digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
  Serial.printf("[Actuator] %-12s → %s\n", label, on?"ON":"OFF");
}
