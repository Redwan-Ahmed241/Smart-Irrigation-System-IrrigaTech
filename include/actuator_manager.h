#pragma once
// =================================================================
//  actuator_manager.h  —  All relay/servo/buzzer control
//  Supports AUTO mode and MANUAL OVERRIDE per actuator (web UI)
// =================================================================
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

// Per-actuator control mode
enum class CtrlMode { AUTO, MANUAL };

class ActuatorManager {
public:
  void begin();

  // ── AUTO mode setters (called by logic/fault/mist controllers) ─
  void setPumpA(bool on);
  void setPumpB(bool on);
  void setFan(bool on);
  void setMist(bool on);
  void setShade(bool open);   // true = open (retracted), false = closed

  // ── Manual override from web UI ───────────────────────────────
  void setManualPumpA(bool on);
  void setManualPumpB(bool on);
  void setManualFan(bool on);
  void setManualMist(bool on);
  void setManualShade(bool open);
  void setMode(const String& actuator, CtrlMode mode);
  CtrlMode getMode(const String& actuator) const;

  // ── Buzzer ─────────────────────────────────────────────────────
  // Call regularly — non-blocking burst pattern
  void buzzerUpdate();
  void buzzerTriggerBurst();   // Trigger a 3-beep burst
  void buzzerStop();           // Silence buzzer

  // ── Emergency ──────────────────────────────────────────────────
  void emergencyStop();        // Gas critical: pumps+mist OFF, fan ON

  // ── Getters ────────────────────────────────────────────────────
  bool isPumpAOn()   const { return _pumpA;  }
  bool isPumpBOn()   const { return _pumpB;  }
  bool isFanOn()     const { return _fan;    }
  bool isMistOn()    const { return _mist;   }
  bool isShadeOpen() const { return _shadeOpen; }

  void printStatus();

private:
  // Actual relay states
  bool _pumpA    = false;
  bool _pumpB    = false;
  bool _fan      = false;
  bool _mist     = false;
  bool _shadeOpen = true;

  // Manual override values
  bool _manPumpA = false;
  bool _manPumpB = false;
  bool _manFan   = false;
  bool _manMist  = false;
  bool _manShade = true;

  // Control modes
  CtrlMode _modePumpA = CtrlMode::AUTO;
  CtrlMode _modePumpB = CtrlMode::AUTO;
  CtrlMode _modeFan   = CtrlMode::AUTO;
  CtrlMode _modeMist  = CtrlMode::AUTO;
  CtrlMode _modeShade = CtrlMode::AUTO;

  // Servo
  Servo _servo;

  // Buzzer state machine
  bool          _buzzerActive   = false;
  uint8_t       _beepsDone      = 0;
  bool          _beepOn         = false;
  unsigned long _buzzerTimer    = 0;

  void _applyRelay(int pin, bool on, const char* label);
  void _applyAll();  // re-apply all states after mode change
};
