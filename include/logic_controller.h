#pragma once
// =================================================================
//  logic_controller.h  —  Pure decision engine, no hardware
// =================================================================
#include <Arduino.h>
#include "sensor_manager.h"
#include "config.h"

struct ActuatorCommands {
  bool pumpA;
  bool pumpB;
  bool fan;
  bool mist;
  bool shadeOpen;
};

class LogicController {
public:
  ActuatorCommands evaluate(const SensorData& d, const ActuatorCommands& prev);

private:
  bool _pump(float pct, bool currentOn);
  bool _fan(const SensorData& d);
  bool _mist(const SensorData& d, bool currentMist);
  bool _shade(const SensorData& d, bool currentShadeOpen);
};
