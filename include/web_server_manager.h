#pragma once
// =================================================================
//  web_server_manager.h
//  Async web server — dashboard + REST API + actuator control
//  Access: http://<ESP32-IP>/
// =================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "fault_monitor.h"
#include "mist_controller.h"
#include "config.h"

class WebServerManager {
public:
  void begin(ActuatorManager* act);
  void update(const SensorData& d,
              const FaultEvent& fault,
              const MistController& mist);

  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
  String localIP()   const { return WiFi.localIP().toString(); }
  bool connectWiFi();

private:
  AsyncWebServer _server{WEB_PORT};
  ActuatorManager* _act = nullptr;

  // Latest data cached for JSON API
  SensorData   _lastData;
  FaultEvent   _lastFault;
  String       _mistState;

  void _setupRoutes();
  String _buildJSON();
  String _buildHTML();
};
