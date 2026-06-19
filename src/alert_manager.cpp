// =================================================================
//  alert_manager.cpp
// =================================================================
#include "alert_manager.h"

void AlertManager::begin(SIM800LManager* sim, ActuatorManager* act) {
  _sim     = sim;
  _act     = act;
  _lastHB  = millis();
  Serial.println(F("[Alert] Initialized."));
}

void AlertManager::handle(FaultEvent& fault) {
  unsigned long now = millis();

  // ── No fault ──────────────────────────────────────────────
  if (fault.code == FaultCode::NONE) {
    if (_lastCode != FaultCode::NONE) {
      // Fault just cleared
      Serial.println(F("[Alert] Fault cleared — stopping buzzer."));
      if (_act) _act->buzzerStop();
      _lastCode = FaultCode::NONE;
    }
    return;
  }

  // ── New fault or changed fault ────────────────────────────
  bool isNewFault = (fault.code != _lastCode);
  if (isNewFault) {
    _lastCode    = fault.code;
    _lastBuzzer  = 0;  // Force immediate buzzer burst
    Serial.printf("[Alert] New fault: code %d\n", (int)fault.code);
  }

  // ── SMS — sent ONCE per fault, never repeated ─────────────
  if (!fault.smsSent) {
    if (_sim && _sim->isReady()) {
      _sim->sendSMS(OWNER_NUMBER, fault.sms);
      Serial.printf("[Alert] SMS sent: %s\n", fault.sms.c_str());
    } else {
      Serial.println(F("[Alert] SIM not ready — SMS skipped."));
    }
    fault.smsSent = true;
  }

  // ── Buzzer — burst on first detect, then every interval ───
  if (_act) {
    bool doBurst = false;
    if (!fault.buzzerTriggered) {
      doBurst = true;
      fault.buzzerTriggered = true;
    } else if (now - _lastBuzzer >= BUZZER_INTERVAL_MS) {
      doBurst = true;
    }
    if (doBurst) {
      _act->buzzerTriggerBurst();
      _lastBuzzer = now;
      Serial.println(F("[Alert] Buzzer burst triggered."));
    }
  }

  // Serial log every fault tick
  Serial.printf("[Alert] ACTIVE FAULT %d: %s\n",
                (int)fault.code, fault.detail.c_str());
}

void AlertManager::checkHeartbeat(int simBars, float temp, float hum,
                                   float soilA, float soilB) {
  if (millis() - _lastHB < INTERVAL_HEARTBEAT) return;
  _lastHB = millis();

  if (!_sim || !_sim->isReady()) return;

  String msg = "GREENHOUSE OK | ";
  msg += "T:" + String(temp, 1) + "C ";
  msg += "H:" + String(hum,  1) + "% ";
  msg += "A:" + String((int)soilA) + "% ";
  msg += "B:" + String((int)soilB) + "% ";
  msg += "Sig:" + String(simBars) + "/5";

  _sim->sendSMS(OWNER_NUMBER, msg);
  // Reset last code so next fault always sends fresh SMS
  _lastCode = FaultCode::NONE;
  Serial.println(F("[Alert] Heartbeat SMS queued."));
}
