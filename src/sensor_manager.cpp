// =================================================================
//  sensor_manager.cpp
// =================================================================
#include "sensor_manager.h"
#include "logic_controller.h"
#include "fault_monitor.h"
#include "esp_task_wdt.h"

void SensorManager::begin() {
  pinMode(PIN_SOIL_A, INPUT);
  pinMode(PIN_SOIL_B, INPUT);
  pinMode(PIN_LDR,    INPUT);
  pinMode(PIN_MQ2,    INPUT);
  _dht.begin();
  // DHT22 needs 2 s after power-on — feed watchdog during wait
  delay(1000); esp_task_wdt_reset();
  delay(1000); esp_task_wdt_reset();
  Serial.println(F("[Sensor] DHT22 + all sensors initialized."));
}

SensorData SensorManager::readAll() {
  SensorData d;
  d.readAt = millis();

  // ── Soil A ───────────────────────────────────────────────────
  d.soilA_raw   = _readAvg(PIN_SOIL_A, 8);
  d.soilA_fault = _soilDisconnected(d.soilA_raw);
  d.soilA_pct   = d.soilA_fault ? -1.0f : _soilToPercent(d.soilA_raw);
  if (d.soilA_fault)
    Serial.println(F("[Sensor] WARN: Soil A disconnected or out of range."));

  // ── Soil B ───────────────────────────────────────────────────
  d.soilB_raw   = _readAvg(PIN_SOIL_B, 8);
  d.soilB_fault = _soilDisconnected(d.soilB_raw);
  d.soilB_pct   = d.soilB_fault ? -1.0f : _soilToPercent(d.soilB_raw);
  if (d.soilB_fault)
    Serial.println(F("[Sensor] WARN: Soil B disconnected or out of range."));

  // ── DHT22 (retry once on failure) ────────────────────────────
  d.temperature = _dht.readTemperature();
  d.humidity    = _dht.readHumidity();
  if (!_validateDHT(d.temperature, d.humidity)) {
    delay(300);
    d.temperature = _dht.readTemperature();
    d.humidity    = _dht.readHumidity();
  }
  bool rawFail  = !_validateDHT(d.temperature, d.humidity);
  d.dhtRawFail  = rawFail;
  d.dhtFault    = _confirmDHTFault(rawFail);  // only true after 2 min sustained

  if (rawFail && !d.dhtFault) {
    Serial.println(F("[Sensor] DHT22 glitch (not confirmed fault yet)."));
  } else if (d.dhtFault) {
    Serial.println(F("[Sensor] DHT22 CONFIRMED FAULT (>2 min)."));
  }

  // ── LDR + MQ-2 ───────────────────────────────────────────────
  d.lightLevel = _readAvg(PIN_LDR, 5);
  d.gasLevel   = _readAvg(PIN_MQ2, 5);

  // ── Overall fault ─────────────────────────────────────────────
  // Only confirmed DHT fault counts — glitches are ignored
  d.anyFault = d.soilA_fault || d.soilB_fault || d.dhtFault;

  return d;
}

void SensorManager::printSerial(const SensorData& d) {
  Serial.println(F("===== Sensor Data ====="));
  if (d.soilA_fault)
    Serial.println(F("  SoilA : DISCONNECTED"));
  else
    Serial.printf( "  SoilA : %d raw → %.1f%%\n", d.soilA_raw, d.soilA_pct);
  if (d.soilB_fault)
    Serial.println(F("  SoilB : DISCONNECTED"));
  else
    Serial.printf( "  SoilB : %d raw → %.1f%%\n", d.soilB_raw, d.soilB_pct);
  if (d.dhtRawFail) {
    Serial.printf( "  Temp  : FAIL%s\n", d.dhtFault ? " [CONFIRMED]" : " [glitch]");
    Serial.printf( "  Hum   : FAIL%s\n", d.dhtFault ? " [CONFIRMED]" : " [glitch]");
  } else {
    Serial.printf( "  Temp  : %.1f C\n",  d.temperature);
    Serial.printf( "  Hum   : %.1f %%\n", d.humidity);
  }
  Serial.printf(  "  Light : %d\n", d.lightLevel);
  Serial.printf(  "  Gas   : %d\n", d.gasLevel);
  Serial.printf(  "  Fault : %s\n", d.anyFault ? "YES" : "no");
  Serial.println(F("======================="));
}

// ── Private ───────────────────────────────────────────────────

// Average N readings, drop min and max (outlier rejection)
int SensorManager::_readAvg(int pin, uint8_t n) {
  int vals[10];
  n = constrain(n, 2, 10);
  for (uint8_t i = 0; i < n; i++) {
    vals[i] = analogRead(pin);
    delay(4);
  }
  // Bubble sort
  for (uint8_t i = 0; i < n - 1; i++)
    for (uint8_t j = 0; j < n - 1 - i; j++)
      if (vals[j] > vals[j+1]) { int t=vals[j]; vals[j]=vals[j+1]; vals[j+1]=t; }
  // Average middle values (drop lowest and highest)
  long sum = 0;
  for (uint8_t i = 1; i < n - 1; i++) sum += vals[i];
  return (int)(sum / (n - 2));
}

// Map raw ADC to moisture percent using calibrated endpoints
// Resistive sensor: dry air = HIGH raw, water = LOW raw → inverted
float SensorManager::_soilToPercent(int raw) {
  // Clamp raw to calibrated range first
  raw = constrain(raw, SOIL_WATER_RAW, SOIL_AIR_RAW);
  float pct = (float)(SOIL_AIR_RAW - raw) /
              (float)(SOIL_AIR_RAW - SOIL_WATER_RAW) * 100.0f;
  return constrain(pct, 0.0f, 100.0f);
}

// Floating/disconnected pin reads very low (near 0)
bool SensorManager::_soilDisconnected(int raw) {
  return (raw < SOIL_DISCONNECT_RAW);
}

// Validate DHT22 reading within physical limits
bool SensorManager::_validateDHT(float t, float h) {
  if (isnan(t) || isnan(h))               return false;
  if (t < TEMP_FAULT_LOW  || t > TEMP_FAULT_HIGH)  return false;
  if (h < HUMIDITY_FAULT_LOW || h > HUMIDITY_FAULT_HIGH) return false;
  return true;
}

// DHT fault confirmation — ignores short glitches
// Only returns true after DHT_FAULT_CONFIRM_MS (2 min) of continuous failure
bool SensorManager::_confirmDHTFault(bool rawFail) {
  if (rawFail) {
    // Sensor is currently failing
    if (!_dhtFailActive) {
      // First failure — start timer
      _dhtFailActive = true;
      _dhtFailSince  = millis();
      Serial.println(F("[Sensor] DHT22 glitch started — waiting to confirm."));
    }
    // Check if failure has been sustained long enough
    if (!_dhtFaultConfirmed &&
        (millis() - _dhtFailSince) >= DHT_FAULT_CONFIRM_MS) {
      _dhtFaultConfirmed = true;
      Serial.printf("[Sensor] DHT22 fault CONFIRMED after %lu s\n",
                    DHT_FAULT_CONFIRM_MS / 1000);
    }
  } else {
    // Sensor is reading OK — reset everything
    if (_dhtFailActive) {
      unsigned long dur = millis() - _dhtFailSince;
      Serial.printf("[Sensor] DHT22 recovered after %lu ms\n", dur);
    }
    _dhtFailActive     = false;
    _dhtFailSince      = 0;
    _dhtFaultConfirmed = false;
  }
  return _dhtFaultConfirmed;
}

// =================================================================
//  printSystemState — full snapshot of sensors + commands + faults
// =================================================================
void printSystemState(const SensorData& d,
                      const ActuatorCommands& cmd,
                      const FaultEvent& fault) {
  Serial.println(F("\n╔══════════════ SYSTEM STATE ══════════════╗"));

  // ── Sensors ──────────────────────────────────
  Serial.println(F("║ SENSORS"));
  if (d.soilA_fault)
    Serial.println(F("║  Soil A : DISCONNECTED           [FAULT]"));
  else
    Serial.printf( "║  Soil A : %5.1f%%  raw=%4d          [%s]\n",
      d.soilA_pct, d.soilA_raw,
      d.soilA_pct < SOIL_DRY_PCT ? "DRY" : d.soilA_pct > SOIL_WET_PCT ? "WET" : "OK ");

  if (d.soilB_fault)
    Serial.println(F("║  Soil B : DISCONNECTED           [FAULT]"));
  else
    Serial.printf( "║  Soil B : %5.1f%%  raw=%4d          [%s]\n",
      d.soilB_pct, d.soilB_raw,
      d.soilB_pct < SOIL_DRY_PCT ? "DRY" : d.soilB_pct > SOIL_WET_PCT ? "WET" : "OK ");

  if (d.dhtFault) {
    Serial.println(F("║  Temp   : FAULT"));
    Serial.println(F("║  Hum    : FAULT"));
  } else {
    Serial.printf( "║  Temp   : %6.1f C                [%s]\n",
      d.temperature, d.temperature > TEMP_HIGH ? "HIGH" : "OK  ");
    Serial.printf( "║  Hum    : %6.1f %%               [%s]\n",
      d.humidity,
      d.humidity < MIST_ON_BELOW  ? "LOW " :
      d.humidity > HUMIDITY_HIGH  ? "HIGH" : "OK  ");
  }
  Serial.printf(  "║  Gas    : %6d  limit=%d      [%s]\n",
    d.gasLevel, GAS_DANGER, d.gasLevel > GAS_DANGER ? "DANGER" : "OK    ");
  Serial.printf(  "║  Light  : %6d  shade@%d   [%s]\n",
    d.lightLevel, LIGHT_DEPLOY_SHADE,
    d.lightLevel > LIGHT_DEPLOY_SHADE ? "HIGH" : "OK  ");

  // ── Commands ──────────────────────────────────
  Serial.println(F("║"));
  Serial.println(F("║ ACTUATOR COMMANDS"));
  Serial.printf( "║  Pump A : %s\n", cmd.pumpA     ? "ON " : "off");
  Serial.printf( "║  Pump B : %s\n", cmd.pumpB     ? "ON " : "off");
  Serial.printf( "║  Fan    : %s\n", cmd.fan       ? "ON " : "off");
  Serial.printf( "║  Mist   : %s\n", cmd.mist      ? "ON " : "off");
  Serial.printf( "║  Shade  : %s\n", cmd.shadeOpen ? "OPEN" : "CLOSED");

  // ── Fault ─────────────────────────────────────
  Serial.println(F("║"));
  if (fault.code == FaultCode::NONE) {
    Serial.println(F("║ FAULT   : none"));
  } else {
    Serial.printf( "║ FAULT   : CODE %d — %s\n",
                   (int)fault.code, fault.detail.c_str());
    Serial.printf( "║  SMS    : %s\n", fault.smsSent         ? "sent" : "pending");
    Serial.printf( "║  Buzzer : %s\n", fault.buzzerTriggered ? "triggered" : "pending");
    Serial.printf( "║  Critical: %s\n", fault.isCritical ? "YES" : "no");
  }

  Serial.println(F("╚══════════════════════════════════════════╝\n"));
}
