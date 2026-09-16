#include "TripLogger.h"
#include <vector>
#include <algorithm>

TripLogger tripLogger;

TripLogger::TripLogger()
  : m_tripId(0),
    m_fileOpen(false),
    m_tripStartTimeMs(0),
    m_lastLogTimeMs(0),
    m_lastFlushTimeMs(0),
    m_recordsLogged(0),
    m_tripDistanceKm(0.0f),
    m_maxSpeedKmh(0.0f),
    m_maxPowerW(0.0f),
    m_lastSpeedKmh(0.0f),
    m_lastSpeedTimeMs(0),
    m_bufLen(0) {
  memset(m_writeBuffer, 0, sizeof(m_writeBuffer));
}

bool TripLogger::begin() {
  Serial.println("[Logger] Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("[Logger] ERROR: LittleFS mount failed!");
    return false;
  }
  Serial.printf("[Logger] LittleFS mounted. Total: %u KB, Used: %u KB, Free: %u KB\n",
                (unsigned int)(getFsTotalBytes() / 1024),
                (unsigned int)(getFsUsedBytes() / 1024),
                (unsigned int)(getFsFreeBytes() / 1024));

  // Retrieve persistent trip counter from NVS
  Preferences prefs;
  prefs.begin("vesc_trip", false);
  m_tripId = prefs.getUInt("trip_id", 0) + 1;
  prefs.putUInt("trip_id", m_tripId);
  prefs.end();

  Serial.printf("[Logger] Power-on detected: Starting New Trip #%u\n", m_tripId);

  // Ensure /trips directory exists
  if (!LittleFS.exists("/trips")) {
    LittleFS.mkdir("/trips");
  }

  // Loop recording: auto-delete oldest logs if MAX_STORED_TRIPS is reached or space is needed
  pruneOldestTrips();

  // Create new trip file
  char path[32];
  snprintf(path, sizeof(path), "/trips/trip_%04u.csv", m_tripId);
  m_currentFile = LittleFS.open(path, FILE_WRITE);
  if (!m_currentFile) {
    Serial.printf("[Logger] ERROR: Failed to open %s for writing!\n", path);
    m_fileOpen = false;
    return false;
  }

  // Write CSV Header directly
  m_currentFile.println("time_s,v_in,current_in,power_w,erpm,speed_kmh,distance_km,ah_used,wh_used,temp_fet,fault");
  m_currentFile.flush();
  
  m_fileOpen = true;
  m_tripStartTimeMs = millis();
  m_lastFlushTimeMs = m_tripStartTimeMs;
  m_lastSpeedTimeMs = m_tripStartTimeMs;
  m_recordsLogged = 0;
  m_bufLen = 0;

  Serial.printf("[Logger] Loop recording active: %s (Buffer: %u bytes, Max trips: %u)\n",
                path, (unsigned int)LOG_BUFFER_SIZE, (unsigned int)MAX_STORED_TRIPS);
  return true;
}

void TripLogger::flushBufferToFile() {
  if (m_fileOpen && m_bufLen > 0) {
    m_currentFile.write((const uint8_t*)m_writeBuffer, m_bufLen);
    m_bufLen = 0;
  }
}

void TripLogger::loop() {
  if (!m_fileOpen) return;

  uint32_t now = millis();
  if (now - m_lastFlushTimeMs >= FLUSH_INTERVAL_MS) {
    flushBufferToFile();
    m_currentFile.flush();
    m_lastFlushTimeMs = now;

    // In-flight loop recording check: if flash free space drops below limit during ride, prune oldest
    if (getFsFreeBytes() < MIN_LITTLEFS_FREE_BYTES) {
      pruneOldestTrips();
    }
  }
}

void TripLogger::logTelemetry(const VescValues &val) {
  if (!m_fileOpen) return;

  uint32_t now = millis();
  float timeSec = (float)(now - m_tripStartTimeMs) / 1000.0f;

  // Integrate distance based on speed and time delta
  if (m_lastSpeedTimeMs > 0 && now > m_lastSpeedTimeMs) {
    float dtHours = (float)(now - m_lastSpeedTimeMs) / 3600000.0f;
    if (dtHours > 0.0f && dtHours < (10.0f / 3600.0f)) { // Max 10s gap to avoid jump
      m_tripDistanceKm += val.speed_kmh * dtHours;
    }
  }
  m_lastSpeedTimeMs = now;
  m_lastSpeedKmh = val.speed_kmh;

  if (val.speed_kmh > m_maxSpeedKmh) m_maxSpeedKmh = val.speed_kmh;
  if (val.power_w > m_maxPowerW) m_maxPowerW = val.power_w;

  // Format CSV row into temporary buffer
  char line[160];
  int lineLen = snprintf(line, sizeof(line),
                         "%.1f,%.1f,%.2f,%.1f,%ld,%.1f,%.3f,%.3f,%.3f,%.1f,%u\n",
                         timeSec,
                         val.v_in,
                         val.current_in,
                         val.power_w,
                         (long)val.rpm,
                         val.speed_kmh,
                         m_tripDistanceKm,
                         val.amp_hours,
                         val.watt_hours,
                         val.temp_fet,
                         val.fault_code);

  if (lineLen <= 0) return;

  // If adding this line exceeds our small RAM buffer, flush buffer to flash first
  if (m_bufLen + (size_t)lineLen >= LOG_BUFFER_SIZE) {
    flushBufferToFile();
  }

  // Copy into small buffer
  if ((size_t)lineLen < LOG_BUFFER_SIZE) {
    memcpy(m_writeBuffer + m_bufLen, line, lineLen);
    m_bufLen += lineLen;
  } else {
    m_currentFile.write((const uint8_t*)line, lineLen);
  }

  m_recordsLogged++;
}

void TripLogger::flush() {
  if (m_fileOpen) {
    flushBufferToFile();
    m_currentFile.flush();
  }
}

uint32_t TripLogger::getTripDurationSec() const {
  if (m_tripStartTimeMs == 0) return 0;
  return (millis() - m_tripStartTimeMs) / 1000;
}

size_t TripLogger::getFsTotalBytes() {
  return LittleFS.totalBytes();
}

size_t TripLogger::getFsUsedBytes() {
  return LittleFS.usedBytes();
}

size_t TripLogger::getFsFreeBytes() {
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  return (total > used) ? (total - used) : 0;
}

void TripLogger::pruneOldestTrips() {
  char currentPath[32];
  snprintf(currentPath, sizeof(currentPath), "/trips/trip_%04u.csv", m_tripId);

  while (true) {
    File root = LittleFS.open("/trips");
    if (!root || !root.isDirectory()) break;

    std::vector<String> tripFiles;
    File file = root.openNextFile();
    while (file) {
      String name = file.path();
      if (name.endsWith(".csv")) {
        tripFiles.push_back(name);
      }
      file = root.openNextFile();
    }
    root.close();

    if (tripFiles.empty()) break;

    std::sort(tripFiles.begin(), tripFiles.end());

    // Check if pruning is needed:
    // 1. More trips than allowed by loop recording (MAX_STORED_TRIPS)
    // 2. Free flash storage is below safe margin
    bool needPruneCount = (tripFiles.size() >= MAX_STORED_TRIPS);
    bool needPruneSpace = (getFsFreeBytes() < MIN_LITTLEFS_FREE_BYTES);

    if (!needPruneCount && !needPruneSpace) {
      break; // Within all limits
    }

    // Find the oldest file that is NOT the currently active trip
    String oldest = "";
    for (const auto &f : tripFiles) {
      if (!f.equals(currentPath)) {
        oldest = f;
        break;
      }
    }

    if (oldest.length() == 0) {
      // Only the current trip file exists, cannot delete active trip
      break;
    }

    Serial.printf("[Logger] Loop recording: auto-deleting oldest log '%s' (stored: %u/%u trips, free: %u KB)\n",
                  oldest.c_str(),
                  (unsigned int)tripFiles.size(),
                  (unsigned int)MAX_STORED_TRIPS,
                  (unsigned int)(getFsFreeBytes() / 1024));
    LittleFS.remove(oldest);
  }
}

void TripLogger::listTrips(Print &out) {
  out.println("\n--- Stored Trip Logs (Loop Recording) ---");
  File root = LittleFS.open("/trips");
  if (!root || !root.isDirectory()) {
    out.println("No /trips directory found.");
    return;
  }

  std::vector<String> tripFiles;
  File file = root.openNextFile();
  while (file) {
    String name = file.path();
    if (name.endsWith(".csv")) {
      tripFiles.push_back(name);
    }
    file = root.openNextFile();
  }
  root.close();

  if (tripFiles.empty()) {
    out.println("No trip logs found.");
    return;
  }

  std::sort(tripFiles.begin(), tripFiles.end());

  out.printf("Loop Limit: %u trips max | Buffer: %u bytes\n",
             (unsigned int)MAX_STORED_TRIPS, (unsigned int)LOG_BUFFER_SIZE);
  out.println("Trip File                | Size (Bytes) | Status");
  out.println("-------------------------+--------------+------------------");
  char currentPath[32];
  snprintf(currentPath, sizeof(currentPath), "/trips/trip_%04u.csv", m_tripId);

  for (const auto &filePath : tripFiles) {
    File f = LittleFS.open(filePath, FILE_READ);
    size_t sz = f ? f.size() : 0;
    if (f) f.close();

    bool isCurrent = filePath.equals(currentPath);
    out.printf("%-24s | %12u | %s\n", filePath.c_str(), (unsigned int)sz, isCurrent ? "RECORDING (Current)" : "Saved");
  }

  out.printf("Storage: %u KB total, %u KB used, %u KB free\n",
             (unsigned int)(getFsTotalBytes() / 1024),
             (unsigned int)(getFsUsedBytes() / 1024),
             (unsigned int)(getFsFreeBytes() / 1024));
  out.println("-----------------------------------------------------------\n");
}

bool TripLogger::dumpTrip(uint32_t id, Print &out) {
  // If id == 0 or matches current, flush buffer first
  if (id == m_tripId || id == 0) {
    flush();
    if (id == 0) id = m_tripId;
  }

  char path[32];
  snprintf(path, sizeof(path), "/trips/trip_%04u.csv", id);

  if (!LittleFS.exists(path)) {
    out.printf("[Logger] Trip file not found: %s\n", path);
    return false;
  }

  File f = LittleFS.open(path, FILE_READ);
  if (!f) {
    out.printf("[Logger] Failed to read: %s\n", path);
    return false;
  }

  out.printf("\n--- BEGIN %s (%u bytes) ---\n", path, (unsigned int)f.size());
  uint8_t buf[256];
  while (f.available()) {
    size_t len = f.read(buf, sizeof(buf));
    out.write(buf, len);
  }
  f.close();
  out.printf("\n--- END %s ---\n", path);
  return true;
}

bool TripLogger::deleteTrip(uint32_t id) {
  if (id == m_tripId) {
    Serial.println("[Logger] Cannot delete currently active trip.");
    return false;
  }
  char path[32];
  snprintf(path, sizeof(path), "/trips/trip_%04u.csv", id);
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
    Serial.printf("[Logger] Deleted %s\n", path);
    return true;
  } else {
    Serial.printf("[Logger] File %s does not exist.\n", path);
    return false;
  }
}

void TripLogger::clearAllTrips() {
  File root = LittleFS.open("/trips");
  if (!root || !root.isDirectory()) return;

  std::vector<String> toDelete;
  File file = root.openNextFile();
  while (file) {
    String name = file.path();
    toDelete.push_back(name);
    file = root.openNextFile();
  }
  root.close();

  char currentPath[32];
  snprintf(currentPath, sizeof(currentPath), "/trips/trip_%04u.csv", m_tripId);

  for (const auto &p : toDelete) {
    if (!p.equals(currentPath)) {
      LittleFS.remove(p);
      Serial.printf("[Logger] Removed %s\n", p.c_str());
    }
  }
  Serial.println("[Logger] All previous trip logs cleared.");
}
