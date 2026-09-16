#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "VescPacket.h"
#include "config.h"

class TripLogger {
public:
  TripLogger();
  
  bool begin();
  void loop();

  // Log a new telemetry reading
  void logTelemetry(const VescValues &val);

  // Status accessors
  uint32_t getCurrentTripId() const { return m_tripId; }
  size_t getRecordsLogged() const { return m_recordsLogged; }
  float getTripDistanceKm() const { return m_tripDistanceKm; }
  float getMaxSpeedKmh() const { return m_maxSpeedKmh; }
  float getMaxPowerW() const { return m_maxPowerW; }
  uint32_t getTripDurationSec() const;
  bool isLogging() const { return m_fileOpen; }

  // LittleFS utilities & log management
  void listTrips(Print &out);
  bool dumpTrip(uint32_t id, Print &out);
  bool deleteTrip(uint32_t id);
  void clearAllTrips();
  size_t getFsTotalBytes();
  size_t getFsUsedBytes();
  size_t getFsFreeBytes();

  // Force flush
  void flush();

private:
  uint32_t m_tripId;
  File m_currentFile;
  bool m_fileOpen;
  
  uint32_t m_tripStartTimeMs;
  uint32_t m_lastLogTimeMs;
  uint32_t m_lastFlushTimeMs;
  size_t m_recordsLogged;

  // Running trip stats
  float m_tripDistanceKm;
  float m_maxSpeedKmh;
  float m_maxPowerW;
  float m_lastSpeedKmh;
  uint32_t m_lastSpeedTimeMs;

  // Small RAM buffer for loop recording writes
  char m_writeBuffer[LOG_BUFFER_SIZE];
  size_t m_bufLen;

  void pruneOldestTrips();
  void flushBufferToFile();
};

extern TripLogger tripLogger;
