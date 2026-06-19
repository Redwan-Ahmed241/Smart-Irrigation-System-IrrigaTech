// =================================================================
//  mist_controller.cpp
// =================================================================
#include "mist_controller.h"

void MistController::begin() {
  _state   = MistState::IDLE;
  _entered = millis();
  Serial.println(F("[Mist] State machine initialized → IDLE"));
}

bool MistController::update(const SensorData& d) {
  if (d.dhtFault) {
    // DHT fault — force mist OFF and stay IDLE
    if (_state == MistState::RUNNING) {
      Serial.println(F("[Mist] DHT fault — forcing mist OFF"));
      _enter(MistState::IDLE);
    }
    return false;
  }
  float hum = d.humidity;

  // Safety: if humidity is already high enough, never run mist
  // This prevents mist turning ON when humidity is OK
  if (hum >= MIST_OFF_ABOVE && _state == MistState::RUNNING) {
    Serial.printf("[Mist] Hum %.1f%% already OK → stopping\n", hum);
    _enter(MistState::COOLDOWN);
  }

  switch (_state) {
    case MistState::IDLE:
      // Only start mist if humidity is consistently low
      // Require humidity to be below threshold for 2 consecutive checks
      if (hum < MIST_ON_BELOW) {
        _lowHumCount++;
        if (_lowHumCount >= 2) {  // 2 sensor reads = ~4 seconds confirmation
          _humAtStart = hum;
          _lowHumCount = 0;
          _enter(MistState::RUNNING);
          Serial.printf("[Mist] Confirmed %.1f%% < %.1f → RUNNING\n", hum, MIST_ON_BELOW);
        }
      } else {
        _lowHumCount = 0;  // Reset counter if humidity recovers
      }
      break;

    case MistState::RUNNING:
      if (hum >= MIST_OFF_ABOVE) {
        Serial.printf("[Mist] %.1f%% ≥ %.1f → COOLDOWN\n", hum, MIST_OFF_ABOVE);
        _enter(MistState::COOLDOWN);
        break;
      }
      if (_elapsed(MIST_MAX_ON_MS)) {
        float rise = hum - _humAtStart;
        if (rise < 2.0f) {
          Serial.printf("[Mist] Timeout, hum only rose %.1f%% → FAULT\n", rise);
          _enter(MistState::FAULT);
        } else {
          Serial.printf("[Mist] Timeout OK (rose %.1f%%) → COOLDOWN\n", rise);
          _enter(MistState::COOLDOWN);
        }
      }
      break;

    case MistState::COOLDOWN:
      if (_elapsed(15000UL)) {
        _enter(MistState::IDLE);
        Serial.println(F("[Mist] Cooldown done → IDLE"));
      }
      break;

    case MistState::FAULT:
      // Stays here until clearFault() is called
      break;
  }
  return isMistOn();
}

unsigned long MistController::onDurationMs() const {
  if (_state == MistState::RUNNING) return millis() - _entered;
  return 0;
}

void MistController::clearFault() {
  Serial.println(F("[Mist] Fault cleared → IDLE"));
  _enter(MistState::IDLE);
}

String MistController::stateName() const {
  switch (_state) {
    case MistState::IDLE:     return "IDLE";
    case MistState::RUNNING:  return "ON";
    case MistState::COOLDOWN: return "COOL";
    case MistState::FAULT:    return "FAULT";
    default:                  return "?";
  }
}

void MistController::_enter(MistState s) { _state = s; _entered = millis(); }
bool MistController::_elapsed(unsigned long ms) const { return (millis()-_entered) >= ms; }
