#pragma once
// =================================================================
//  mist_controller.h  —  Ultrasonic mist maker state machine
//  IDLE → RUNNING → COOLDOWN → FAULT
// =================================================================
#include <Arduino.h>
#include "sensor_manager.h"
#include "config.h"

enum class MistState { IDLE, RUNNING, COOLDOWN, FAULT };

class MistController {
public:
  void      begin();
  bool      update(const SensorData& d);   // returns true = mist ON
  bool      isMistOn()  const { return _state == MistState::RUNNING; }
  bool      hasFault()  const { return _state == MistState::FAULT;   }
  MistState getState()  const { return _state; }
  String    stateName() const;
  unsigned long onDurationMs() const;
  void      clearFault();

private:
  MistState     _state      = MistState::IDLE;
  unsigned long _entered    = 0;
  float         _humAtStart = 0;
  uint8_t       _lowHumCount = 0;  // consecutive low-humidity reads before mist starts

  void _enter(MistState s);
  bool _elapsed(unsigned long ms) const;
};
