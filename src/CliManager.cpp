#include "CliManager.h"
#include "TripLogger.h"
#include "WebPortal.h"

CliManager cliManager;

CliManager::CliManager() {
  m_inputBuffer.reserve(64);
}

void CliManager::loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      m_inputBuffer.trim();
      if (m_inputBuffer.length() > 0) {
        handleCommand(m_inputBuffer);
      }
      m_inputBuffer = "";
    } else {
      if (m_inputBuffer.length() < 64) {
        m_inputBuffer += c;
      }
    }
  }
}

void CliManager::handleCommand(const String &cmdLine) {
  String cmd = cmdLine;
  String arg = "";
  int spaceIdx = cmdLine.indexOf(' ');
  if (spaceIdx > 0) {
    cmd = cmdLine.substring(0, spaceIdx);
    arg = cmdLine.substring(spaceIdx + 1);
    arg.trim();
  }
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") {
    Serial.println("\n=== VESC Trip Logger Commands ===");
    Serial.println("  status       : View current logging status and flash storage");
    Serial.println("  trips / list : List all recorded trip files on LittleFS");
    Serial.println("  dump [id]    : Print CSV data of trip [id] (e.g. 'dump 1' or 'dump')");
    Serial.println("  del <id>     : Delete a specific trip file (e.g. 'del 1')");
    Serial.println("  clear        : Delete all previous trip logs");
    Serial.println("  wifi         : Start Wi-Fi Access Point to download logs via web browser");
    Serial.println("  nowifi       : Turn off Wi-Fi Access Point");
    Serial.println("  help         : Show this help menu\n");
  } else if (cmd == "status") {
    Serial.println("\n--- VESC Bridge & Trip Status ---");
    Serial.printf("Current Trip ID : #%u\n", tripLogger.getCurrentTripId());
    Serial.printf("Trip Duration   : %u seconds\n", (unsigned int)tripLogger.getTripDurationSec());
    Serial.printf("Records Logged  : %u\n", (unsigned int)tripLogger.getRecordsLogged());
    Serial.printf("Trip Distance   : %.2f km\n", tripLogger.getTripDistanceKm());
    Serial.printf("Max Speed       : %.1f km/h\n", tripLogger.getMaxSpeedKmh());
    Serial.printf("Max Power       : %.1f W\n", tripLogger.getMaxPowerW());
    Serial.printf("Flash Free      : %u KB / %u KB\n",
                  (unsigned int)(tripLogger.getFsFreeBytes() / 1024),
                  (unsigned int)(tripLogger.getFsTotalBytes() / 1024));
    Serial.printf("Web Portal      : %s\n", webPortal.isRunning() ? "RUNNING" : "STOPPED");
    Serial.println("--------------------------------\n");
  } else if (cmd == "trips" || cmd == "list") {
    tripLogger.listTrips(Serial);
  } else if (cmd == "dump" || cmd == "cat") {
    uint32_t id = 0;
    if (arg.length() > 0) {
      id = arg.toInt();
    }
    tripLogger.dumpTrip(id, Serial);
  } else if (cmd == "del" || cmd == "delete") {
    if (arg.length() == 0) {
      Serial.println("[CLI] Usage: del <trip_id>");
    } else {
      uint32_t id = arg.toInt();
      tripLogger.deleteTrip(id);
    }
  } else if (cmd == "clear") {
    tripLogger.clearAllTrips();
  } else if (cmd == "wifi") {
    webPortal.start();
  } else if (cmd == "nowifi") {
    webPortal.stop();
  } else {
    Serial.printf("[CLI] Unknown command: '%s'. Type 'help' for commands.\n", cmd.c_str());
  }
}
