#pragma once

#include <Arduino.h>
#include <DNSServer.h>

class WebPortal {
public:
  WebPortal();
  bool start();
  void stop();
  void loop();
  bool isRunning() const { return m_running; }

private:
  bool m_running;
  DNSServer *m_dnsServer;
  uint32_t m_startTime;
  uint32_t m_lastStationActiveTime;
  bool m_hasHadClient;
};

extern WebPortal webPortal;
