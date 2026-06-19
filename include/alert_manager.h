#pragma once
// =================================================================
//  alert_manager.h
//  - SMS sent ONCE per fault (never repeated)
//  - Buzzer burst triggered immediately on new fault
//  - Buzzer repeats every BUZZER_INTERVAL_MS while fault persists
//  - Hourly heartbeat SMS when system is healthy
// =================================================================
#include <Arduino.h>
#include "fault_monitor.h"
#include "sim800l_manager.h"
#include "actuator_manager.h"

class AlertManager {
public:
  void begin(SIM800LManager* sim, ActuatorManager* act);

  // Call every loop tick
  void handle(FaultEvent& fault);

  // Call every loop — sends heartbeat SMS once per hour
  void checkHeartbeat(int simBars, float temp, float hum,
                      float soilA, float soilB);

private:
  SIM800LManager* _sim         = nullptr;
  ActuatorManager* _act        = nullptr;
  FaultCode        _lastCode   = FaultCode::NONE;
  unsigned long    _lastHB     = 0;
  unsigned long    _lastBuzzer = 0;   // Last buzzer burst time
};
