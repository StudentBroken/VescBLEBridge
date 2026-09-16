#pragma once

#include <Arduino.h>

class WebPortal {
public:
  WebPortal();
  bool start();
  void stop();
  void loop();
  bool isRunning() const { return m_running; }

private:
  bool m_running;
};

extern WebPortal webPortal;
