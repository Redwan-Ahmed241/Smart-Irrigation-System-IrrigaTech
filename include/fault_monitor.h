#pragma once
// =================================================================
//  fault_monitor.h  — FIXED
//  SMS sent ONCE per fault, never repeated for same code.
//  Buzzer burst triggered ONCE per fault, then on interval only.
// =================================================================
#include <Arduino.h>
#include "sensor_manager.h"
#include "config.h"

enum class FaultCode {
  NONE          = 0,
  GAS_CRITICAL  = 1,
  SENSOR_FAIL   = 2,
  PUMP_A_STUCK  = 3,
  PUMP_B_STUCK  = 4,
  MIST_STUCK    = 5,
  SOIL_A_DISC   = 6,
  SOIL_B_DISC   = 7,
};

struct FaultEvent {
  FaultCode code            = FaultCode::NONE;
  String    detail;
  String    sms;
  bool      isCritical      = false;
  bool      smsSent         = false;   // Set true by alert_manager — NEVER reset by fault_monitor
  bool      buzzerTriggered = false;   // Set true by alert_manager — NEVER reset by fault_monitor
  unsigned long detectedAt  = 0;
};

class FaultMonitor {
public:
  void begin();

  FaultEvent check(const SensorData& d,
                   bool pumpAon, unsigned long pumpAms,
                   bool pumpBon, unsigned long pumpBms,
                   bool miston,  unsigned long mistms);

  bool       hasFault()   const { return _active.code != FaultCode::NONE; }
  FaultCode  activeCode() const { return _active.code; }
  FaultEvent& active()          { return _active; }
  void       clear();

private:
  FaultEvent    _active;
  unsigned long _gasFirstSeen = 0;
  bool          _gasTracking  = false;

  // Step 1: which fault code is active right now?
  FaultCode _detectCode(const SensorData& d,
                        bool pumpAon, unsigned long pumpAms,
                        bool pumpBon, unsigned long pumpBms,
                        bool miston,  unsigned long mistms);

  // Step 2: build full FaultEvent when NEW fault appears
  FaultEvent _make(FaultCode code, const SensorData& d,
                   unsigned long pumpAms, unsigned long pumpBms,
                   unsigned long mistms);

  // Step 3: update detail text only for persisting fault (never touches smsSent)
  String _buildDetail(FaultCode code, const SensorData& d,
                      unsigned long pumpAms, unsigned long pumpBms,
                      unsigned long mistms);
};
