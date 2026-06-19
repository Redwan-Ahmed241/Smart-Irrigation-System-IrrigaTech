// =================================================================
//  fault_monitor.cpp  — FIXED
//
//  Root cause of SMS spam:
//    _make() was called every loop tick → always created fresh event
//    with smsSent=false → _mergePersist saw matching code but
//    the new event detail string changed (seconds counter) so
//    it looked like a new fault each time.
//
//  Fix: _make() only called ONCE when fault first appears.
//    Subsequent ticks just update the detail string on _active
//    directly — smsSent/buzzerTriggered flags are NEVER reset.
// =================================================================
#include "fault_monitor.h"

void FaultMonitor::begin() {
  _active      = {};
  _gasTracking = false;
  Serial.println(F("[Fault] Monitor ready."));
}

FaultEvent FaultMonitor::check(const SensorData& d,
                                bool pumpAon, unsigned long pumpAms,
                                bool pumpBon, unsigned long pumpBms,
                                bool miston,  unsigned long mistms) {
#if !FAULT_DETECTION_ENABLED
  _active = {};
  return _active;
#endif

  FaultCode detected = _detectCode(d, pumpAon, pumpAms,
                                       pumpBon, pumpBms,
                                       miston,  mistms);

  if (detected == FaultCode::NONE) {
    // ── All clear ─────────────────────────────────────────
    if (_active.code != FaultCode::NONE)
      Serial.println(F("[Fault] Cleared."));
    _active = {};
    return _active;
  }

  if (detected != _active.code) {
    // ── New fault type — create fresh event ONCE ──────────
    _active = _make(detected, d, pumpAms, pumpBms, mistms);
    Serial.printf("[Fault] NEW CODE %d: %s\n",
                  (int)_active.code, _active.detail.c_str());
  } else {
    // ── Same fault persisting — only update detail text ───
    // smsSent and buzzerTriggered are NEVER touched here
    _active.detail = _buildDetail(detected, d, pumpAms, pumpBms, mistms);
  }

  return _active;
}

void FaultMonitor::clear() { _active = {}; }

// =================================================================
//  Step 1: Detect which fault code is active (no side effects)
// =================================================================
FaultCode FaultMonitor::_detectCode(const SensorData& d,
                                     bool pumpAon, unsigned long pumpAms,
                                     bool pumpBon, unsigned long pumpBms,
                                     bool miston,  unsigned long mistms) {
  // Gas — must sustain GAS_CONFIRM_MS to avoid spikes
  if (d.gasLevel > GAS_DANGER) {
    if (!_gasTracking) { _gasTracking = true; _gasFirstSeen = millis(); }
    if ((millis() - _gasFirstSeen) >= GAS_CONFIRM_MS)
      return FaultCode::GAS_CRITICAL;
  } else {
    _gasTracking = false; _gasFirstSeen = 0;
  }

  // DHT22 sensor fail — only confirmed faults (glitches ignored)
  if (d.dhtFault) return FaultCode::SENSOR_FAIL;

  // Soil disconnected
  if (d.soilA_fault) return FaultCode::SOIL_A_DISC;
  if (d.soilB_fault) return FaultCode::SOIL_B_DISC;

  // Pump A stuck — ON too long, soil still dry
  if (pumpAon && pumpAms > PUMP_MAX_ON_MS &&
      !d.soilA_fault && d.soilA_pct < SOIL_DRY_PCT)
    return FaultCode::PUMP_A_STUCK;

  // Pump B stuck — ON too long, soil still dry
  if (pumpBon && pumpBms > PUMP_MAX_ON_MS &&
      !d.soilB_fault && d.soilB_pct < SOIL_DRY_PCT)
    return FaultCode::PUMP_B_STUCK;

  // Mist stuck — ON too long
  if (miston && mistms > MIST_MAX_ON_MS)
    return FaultCode::MIST_STUCK;

  return FaultCode::NONE;
}

// =================================================================
//  Step 2: Build fresh FaultEvent when a NEW fault code appears
//  smsSent and buzzerTriggered start as false — set once here only
// =================================================================
FaultEvent FaultMonitor::_make(FaultCode code, const SensorData& d,
                                unsigned long pumpAms, unsigned long pumpBms,
                                unsigned long mistms) {
  FaultEvent ev;
  ev.code            = code;
  ev.smsSent         = false;   // Will be set true by alert_manager
  ev.buzzerTriggered = false;   // Will be set true by alert_manager
  ev.detectedAt      = millis();

  switch (code) {
    case FaultCode::GAS_CRITICAL:
      ev.detail     = "CRITICAL: Gas=" + String(d.gasLevel) + " Fan ON!";
      ev.sms        = "GREENHOUSE CRITICAL: Dangerous gas detected! Fan ON. Check now!";
      ev.isCritical = true;
      break;
    case FaultCode::SENSOR_FAIL:
      ev.detail = "DHT22 sensor failed!";
      ev.sms    = "GREENHOUSE: DHT22 fault. Auto-control paused. Check sensor.";
      break;
    case FaultCode::SOIL_A_DISC:
      ev.detail = "Soil A disconnected!";
      ev.sms    = "GREENHOUSE: Soil A sensor disconnected. Check wiring.";
      break;
    case FaultCode::SOIL_B_DISC:
      ev.detail = "Soil B disconnected!";
      ev.sms    = "GREENHOUSE: Soil B sensor disconnected. Check wiring.";
      break;
    case FaultCode::PUMP_A_STUCK:
      ev.detail = "Pump A stuck! Soil A still dry";
      ev.sms    = "GREENHOUSE: Zone A pump fault! Ran >60s, soil still dry. Check pump/pipe.";
      break;
    case FaultCode::PUMP_B_STUCK:
      ev.detail = "Pump B stuck! Soil B still dry";
      ev.sms    = "GREENHOUSE: Zone B pump fault! Ran >60s, soil still dry. Check pump/pipe.";
      break;
    case FaultCode::MIST_STUCK:
      ev.detail = "Mist stuck! Hum not rising";
      ev.sms    = "GREENHOUSE: Mist maker fault! Running too long. Check ultrasonic disk.";
      break;
    default:
      break;
  }
  return ev;
}

// =================================================================
//  Update only the detail string for a persisting fault
//  Called every tick for same fault — does NOT touch smsSent
// =================================================================
String FaultMonitor::_buildDetail(FaultCode code, const SensorData& d,
                                   unsigned long pumpAms, unsigned long pumpBms,
                                   unsigned long mistms) {
  switch (code) {
    case FaultCode::GAS_CRITICAL:
      return "CRITICAL: Gas=" + String(d.gasLevel) + " Fan ON!";
    case FaultCode::PUMP_A_STUCK:
      return "Pump A: " + String((int)(pumpAms/1000)) + "s ON, soil "
             + String(d.soilA_pct, 0) + "% (dry)";
    case FaultCode::PUMP_B_STUCK:
      return "Pump B: " + String((int)(pumpBms/1000)) + "s ON, soil "
             + String(d.soilB_pct, 0) + "% (dry)";
    case FaultCode::MIST_STUCK:
      return "Mist: " + String((int)(mistms/1000)) + "s ON, hum "
             + String(d.humidity, 1) + "%";
    default:
      return _active.detail;  // Static faults keep original text
  }
}
