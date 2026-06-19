#pragma once
// =================================================================
//  sim800l_manager.h  —  SIM800L GSM via Serial2, non-blocking
// =================================================================
#include <Arduino.h>
#include "config.h"

class SIM800LManager {
public:
  void  begin();
  void  update();
  bool  sendSMS(const String& number, const String& msg);
  bool  isReady()  const { return _ready; }
  bool  isBusy()   const { return _busy;  }
  int   signalBars();

private:
  bool   _ready      = false;
  bool   _busy       = false;
  bool   _hasPending = false;
  String _pendNum;
  String _pendMsg;

  bool   _handshake();
  bool   _doSend(const String& num, const String& msg);
  bool   _atCmd(const String& cmd, const String& expect, unsigned long timeout = SIM_CMD_TIMEOUT);
  String _atQuery(const String& cmd, unsigned long timeout = SIM_CMD_TIMEOUT);
  void   _flush();
  bool   _waitFor(const String& token, unsigned long timeout);
};
