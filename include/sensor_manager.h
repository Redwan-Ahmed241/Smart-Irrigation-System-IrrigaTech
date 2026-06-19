#pragma once
// =================================================================
//  sensor_manager.h  —  All sensor reads → SensorData struct
//  DHT22, 2x soil, LDR, MQ-2
// =================================================================
#include <Arduino.h>
#include <DHT.h>
#include "config.h"

struct SensorData {
  // Soil Zone A
  int   soilA_raw;
  float soilA_pct;       // 0–100 %moisture, -1 = disconnected
  bool  soilA_fault;     // true = disconnected / out of range

  // Soil Zone B
  int   soilB_raw;
  float soilB_pct;
  bool  soilB_fault;

  // DHT22
  float temperature;     // °C, NAN on fail
  float humidity;        // %RH, NAN on fail
  bool  dhtRawFail;      // true = this read failed (may be a glitch)
  bool  dhtFault;        // true = sustained failure > DHT_FAULT_CONFIRM_MS (real fault)

  // Environment
  int   lightLevel;      // ADC 0–4095  (HIGH=dark, LOW=bright for your LDR)
  int   gasLevel;        // ADC 0–4095

  // Overall — only true when dhtFault (confirmed) or soil fault
  bool  anyFault;
  unsigned long readAt;
};

class SensorManager {
public:
  void       begin();
  SensorData readAll();
  void       printSerial(const SensorData& d);

private:
  DHT _dht{PIN_DHT, DHT22};

  // DHT fault confirmation state
  bool          _dhtFailActive  = false;  // currently failing?
  unsigned long _dhtFailSince   = 0;      // millis() when failure started
  bool          _dhtFaultConfirmed = false; // true after DHT_FAULT_CONFIRM_MS

  int   _readAvg(int pin, uint8_t n = 8);
  float _soilToPercent(int raw);
  bool  _soilDisconnected(int raw);
  bool  _validateDHT(float t, float h);
  bool  _confirmDHTFault(bool rawFail);   // returns true only after sustained failure
};

// ── Free function: full system debug snapshot ──────────────────
// Include logic_controller.h and fault_monitor.h before calling
struct ActuatorCommands;
struct FaultEvent;
void printSystemState(const SensorData& d,
                      const ActuatorCommands& cmd,
                      const FaultEvent& fault);
