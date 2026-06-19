#pragma once
// =================================================================
//  config.h  —  ALL pins, thresholds, flags in ONE place
//  Edit ONLY this file — never hardcode values in .cpp files
// =================================================================

// ── Feature flags (1 = enabled, 0 = disabled) ─────────────────
#define FAULT_DETECTION_ENABLED   1   // 0 = disable all fault detection
#define BUZZER_ENABLED            1   // 0 = silent mode
#define WIFI_ENABLED              1   // 0 = no WiFi / no web server
#define SIM800L_ENABLED           1   // 0 = no GSM / no SMS
#define OLED_ENABLED              1   // 0 = no display

// ── Sensor input pins ──────────────────────────────────────────
#define PIN_SOIL_A          34    // Zone A soil moisture (analog)
#define PIN_SOIL_B          35    // Zone B soil moisture (analog)
#define PIN_DHT             18    // DHT22 data pin
#define PIN_LDR             32    // LDR light sensor (analog)
#define PIN_MQ2             33    // MQ-2 gas sensor (analog)

// ── Actuator output pins ───────────────────────────────────────
#define PIN_PUMP_A          13    // Zone A relay
#define PIN_PUMP_B          13    // Zone B relay
#define PIN_FAN             2    // Fan relay
#define PIN_MIST            27    // Ultrasonic mist maker relay
#define PIN_SERVO_SHADE     23    // Shade servo
#define PIN_BUZZER          25   // Active buzzer (change if needed)

// ── SIM800L UART ───────────────────────────────────────────────
#define PIN_SIM_TX          17
#define PIN_SIM_RX          16
#define SIM_BAUD            9600

// ── OLED I2C ──────────────────────────────────────────────────
#define OLED_SDA            21
#define OLED_SCL            22
#define OLED_ADDR         0x3C
#define OLED_W             128
#define OLED_H              64

// ── Relay polarity ─────────────────────────────────────────────
//  Most relay boards: RELAY_ON = LOW (active low)
//  If your relays are active HIGH, swap these
#define RELAY_ON           HIGH
#define RELAY_OFF          LOW


// ── Servo positions ────────────────────────────────────────────
#define SHADE_OPEN_DEG       0    // Shade retracted
#define SHADE_CLOSED_DEG    90    // Shade deployed

// ── Soil moisture calibration (ADC 0–4095) ────────────────────
//  HOW TO CALIBRATE:
//  1. Put sensor in open air  → note raw value → set SOIL_AIR_RAW
//  2. Put sensor in water     → note raw value → set SOIL_WATER_RAW
//  3. SOIL_AIR_RAW > SOIL_WATER_RAW  (resistive sensor: dry=high)
#define SOIL_AIR_RAW        3400  // Raw when sensor is in dry air
#define SOIL_WATER_RAW       800  // Raw when sensor is in water

// Soil decision thresholds (in percent 0-100)
#define SOIL_DRY_PCT        35    // Below this → pump ON
#define SOIL_WET_PCT        70    // Above this → pump OFF
// Between 35–70% → hysteresis (keep previous state)

// Disconnected pin detection — floating pin reads very low
#define SOIL_DISCONNECT_RAW  80   // Raw below this = sensor unplugged

// ── Temperature thresholds (°C) ────────────────────────────────
#define TEMP_HIGH           35.0f   // Above → fan ON
#define TEMP_LOW            10.0f   // Below → frost warning (future)
#define TEMP_FAULT_HIGH     65.0f   // Above → sensor fault (DHT22 max 80°C)
#define TEMP_FAULT_LOW     -40.0f   // Below → sensor fault

// ── Humidity thresholds (%RH) ──────────────────────────────────
#define HUMIDITY_LOW        40.0f   // Below → fan ON (too dry)
#define HUMIDITY_HIGH       80.0f   // Above → fan ON (too humid)
#define HUMIDITY_FAULT_LOW   0.0f
#define HUMIDITY_FAULT_HIGH 100.0f

// ── Mist maker thresholds (%RH) ────────────────────────────────
#define MIST_ON_BELOW       50.0f   // Start mist when humidity < this
#define MIST_OFF_ABOVE      60.0f   // Stop mist when humidity > this
// Hysteresis band: 50–60% → keep previous state

// ── Light threshold (ADC 0–4095) ───────────────────────────────
//  YOUR LDR: HIGH value (4000) = DARK, LOW value (1000) = BRIGHT
//  Shade deploys when light is BRIGHT (low ADC value)
#define LIGHT_DEPLOY_SHADE  1500    // Below this → bright → deploy shade
#define LIGHT_RETRACT_SHADE 2500    // Above this → dark  → retract shade
// Hysteresis: 1500–2500 → keep previous state

// ── DHT22 fault confirmation ────────────────────────────────────
//  DHT22 occasionally glitches for 1-2 reads — not a real fault.
//  Only raise SENSOR_FAIL after sustained failure for this long:
#define DHT_FAULT_CONFIRM_MS  120000UL  // 2 minutes of continuous fail = real fault

// ── Mist maker pin logic ────────────────────────────────────────
//  Ultrasonic mist maker: LOW pin = ON, HIGH pin = OFF
//  This is opposite of normal relays — handled in actuator_manager
#define MIST_PIN_ON   LOW    // Mist maker activates when pin is LOW
#define MIST_PIN_OFF  HIGH   // Mist maker off when pin is HIGH

// ── Gas threshold (ADC 0–4095) ─────────────────────────────────
#define GAS_DANGER          1500    // Above → fan ON + alert
#define GAS_CLEAR            800    // Below → all clear
#define GAS_CONFIRM_MS      5000UL  // Must persist 5 s to avoid spikes

// ── Fault detection timing ─────────────────────────────────────
#define PUMP_MAX_ON_MS      60000UL  // Pump on > 60s with soil still dry → fault
#define MIST_MAX_ON_MS      45000UL  // Mist on > 45s, hum not rising → fault

// ── Buzzer behaviour ───────────────────────────────────────────
#define BUZZER_BEEP_MS        150   // Single beep duration
#define BUZZER_PAUSE_MS       120   // Pause between beeps in a burst
#define BUZZER_BURST_COUNT      3   // Beeps per burst (3 short beeps)
#define BUZZER_INTERVAL_MS  30000UL // Repeat burst every 30 s until fault clears

// ── Loop / display intervals ───────────────────────────────────
#define INTERVAL_SENSOR     2000UL   // Read sensors every 2 s
#define INTERVAL_DISPLAY    2000UL   // OLED refresh every 2 s
#define INTERVAL_WEB        1000UL   // Web data JSON refresh every 1 s
#define INTERVAL_HEARTBEAT  3600000UL // SMS heartbeat every 1 hour

// ── WiFi credentials ───────────────────────────────────────────
#define WIFI_SSID   "No Internet"     // ← Replace
#define WIFI_PASS   "UiU@2025"     // ← Replace
#define WEB_PORT    80


// ── GSM owner number ───────────────────────────────────────────
#define OWNER_NUMBER  "+8801XXXXXXXXX"  // ← Replace
#define SIM_CMD_TIMEOUT  5000UL
