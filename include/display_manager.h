#pragma once
// =================================================================
//  display_manager.h  —  SSD1306 128x64 OLED, 4 rotating pages
//  Fault screen overrides all pages with flashing warning
// =================================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "fault_monitor.h"
#include "mist_controller.h"
#include "config.h"

class DisplayManager {
public:
  void begin();
  void update(const SensorData& d,
              const ActuatorManager& act,
              const MistController& mist,
              const FaultEvent& fault,
              int simBars,
              bool wifiOk);
  void showBoot();

private:
  Adafruit_SSD1306 _oled{OLED_W, OLED_H, &Wire, -1};
  uint8_t       _page      = 0;
  unsigned long _pageTimer  = 0;
  static const unsigned long PAGE_MS = 3000;

  void _clear();
  void _show();
  void _header(const char* title);
  void _statusBar(int simBars, bool wifiOk);

  void _pageClimate(const SensorData& d, const MistController& mist);
  void _pageSoil(const SensorData& d, bool pumpA, bool pumpB);
  void _pageActuators(const ActuatorManager& act);
  void _pageEnv(const SensorData& d);
  void _pageFault(const FaultEvent& ev);
};
