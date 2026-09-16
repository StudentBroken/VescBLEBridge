#pragma once

#include <Arduino.h>

class CliManager {
public:
  CliManager();
  void loop();

private:
  String m_inputBuffer;
  void handleCommand(const String &cmdLine);
};

extern CliManager cliManager;
