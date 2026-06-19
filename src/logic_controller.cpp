// =================================================================
//  logic_controller.cpp
// =================================================================
#include "logic_controller.h"

ActuatorCommands LogicController::evaluate(const SensorData& d,
                                            const ActuatorCommands& prev) {
  // Only confirmed faults trigger safe default — glitches are ignored
  if (d.anyFault) {
    Serial.println(F("[Logic] Confirmed fault → safe default."));
    return {false, false, false, false, true};
  }

  // Gas danger → override: pumps+mist OFF, fan ON
  if (d.gasLevel > GAS_DANGER) {
    Serial.printf("[Logic] Gas danger %d → emergency.\n", d.gasLevel);
    return {false, false, true, false, true};
  }

  ActuatorCommands cmd;
  cmd.pumpA     = d.soilA_fault ? false : _pump(d.soilA_pct, prev.pumpA);
  cmd.pumpB     = d.soilB_fault ? false : _pump(d.soilB_pct, prev.pumpB);

  // If DHT raw glitch (not confirmed fault) — keep previous fan/mist state
  // This prevents false shutdowns on a 1-2 read glitch
  if (d.dhtRawFail && !d.dhtFault) {
    Serial.println(F("[Logic] DHT glitch — holding previous fan/mist state."));
    cmd.fan       = prev.fan;
    cmd.mist      = prev.mist;
  } else {
    cmd.fan  = _fan(d);
    cmd.mist = _mist(d, prev.mist);
  }

  cmd.shadeOpen = _shade(d, prev.shadeOpen);
  return cmd;
}

// Pump: ON when soil < DRY%, OFF when soil > WET%, hysteresis in between
bool LogicController::_pump(float pct, bool currentOn) {
  if (pct < SOIL_DRY_PCT) return true;   // Dry → pump ON
  if (pct > SOIL_WET_PCT) return false;  // Wet → pump OFF
  return currentOn;                       // Band → keep previous
}

// Fan: ON when temp high OR humidity out of healthy range OR gas high
bool LogicController::_fan(const SensorData& d) {
  if (d.dhtFault) return false;  // confirmed fault only
  bool tempHigh = d.temperature > TEMP_HIGH;
  bool humHigh  = d.humidity    > HUMIDITY_HIGH;
  bool humLow   = d.humidity    < HUMIDITY_LOW;
  bool gasHigh  = d.gasLevel    > GAS_DANGER;
  if (tempHigh) Serial.printf("[Logic] Temp %.1f°C > %.1f → Fan ON\n", d.temperature, TEMP_HIGH);
  if (humHigh)  Serial.printf("[Logic] Hum  %.1f%% > %.1f → Fan ON\n", d.humidity, HUMIDITY_HIGH);
  if (humLow)   Serial.printf("[Logic] Hum  %.1f%% < %.1f → Fan ON\n", d.humidity, HUMIDITY_LOW);
  return (tempHigh || humHigh || humLow || gasHigh);
}

// Mist: ON below MIST_ON_BELOW, OFF above MIST_OFF_ABOVE, hysteresis band
bool LogicController::_mist(const SensorData& d, bool currentMist) {
  if (d.dhtFault) return false;  // confirmed fault only
  if (d.humidity < MIST_ON_BELOW)  return true;
  if (d.humidity > MIST_OFF_ABOVE) return false;
  return currentMist;
}

// Shade: YOUR LDR reads HIGH when dark, LOW when bright
// Deploy shade (close) when light is LOW (bright)
// Retract shade (open) when light is HIGH (dark)
bool LogicController::_shade(const SensorData& d, bool currentOpen) {
  if (d.lightLevel < LIGHT_DEPLOY_SHADE)  return false; // bright (low ADC) → close shade
  if (d.lightLevel > LIGHT_RETRACT_SHADE) return true;  // dark  (high ADC) → open shade
  return currentOpen;  // hysteresis band → keep previous
}
