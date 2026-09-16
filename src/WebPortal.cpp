#include "WebPortal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>
#include "TripLogger.h"
#include "config.h"

static WebServer *server = nullptr;
WebPortal webPortal;

WebPortal::WebPortal() : m_running(false) {}

bool WebPortal::start() {
  if (m_running) return true;

  Serial.println("\n[WiFi] Starting Access Point: " WIFI_AP_SSID);
  WiFi.mode(WIFI_AP);
  if (strlen(WIFI_AP_PASSWORD) >= 8) {
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  } else {
    WiFi.softAP(WIFI_AP_SSID);
  }

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] Access Point started!\n");
  Serial.printf("[WiFi] Connect to Wi-Fi SSID '%s' and open http://%s in your browser to view and download trip CSVs.\n\n",
                WIFI_AP_SSID, ip.toString().c_str());

  if (!server) {
    server = new WebServer(80);
  }

  server->on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>VESC Trip Logs</title><style>";
    html += "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:#0f172a;color:#f8fafc;padding:20px;margin:0}";
    html += ".container{max-width:760px;margin:0 auto}";
    html += "h1{color:#38bdf8;margin-bottom:6px;display:flex;align-items:center;gap:8px}";
    html += ".sub{color:#94a3b8;font-size:14px;margin-bottom:20px}";
    html += ".card{background:#1e293b;border-radius:12px;padding:18px;margin-bottom:18px;box-shadow:0 4px 14px rgba(0,0,0,0.3);border:1px solid #334155}";
    html += "table{width:100%;border-collapse:collapse;margin-top:10px}";
    html += "th,td{padding:12px 10px;text-align:left;border-bottom:1px solid #334155}";
    html += "th{color:#94a3b8;font-size:12px;text-transform:uppercase;letter-spacing:0.5px}";
    html += "a.btn{display:inline-block;padding:7px 14px;border-radius:6px;text-decoration:none;font-size:13px;font-weight:600;transition:all 0.2s}";
    html += ".btn-dl{background:#0284c7;color:#fff}.btn-dl:hover{background:#0369a1}";
    html += ".btn-del{background:#ef4444;color:#fff;margin-left:6px}.btn-del:hover{background:#dc2626}";
    html += ".badge{background:#10b981;color:#fff;padding:3px 9px;border-radius:12px;font-size:11px;font-weight:600}";
    html += ".stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-top:12px}";
    html += ".stat-box{background:#0f172a;padding:12px;border-radius:8px;border:1px solid #334155}";
    html += ".stat-title{font-size:11px;color:#94a3b8;text-transform:uppercase}";
    html += ".stat-val{font-size:18px;font-weight:700;color:#f1f5f9;margin-top:4px}";
    html += "</style></head><body><div class='container'>";
    html += "<h1>⚡ VESC Trip Logger</h1>";
    html += "<div class='sub'>Auto-recorded ride telemetry stored directly on your ESP32-C3 flash.</div>";

    html += "<div class='card'>";
    html += "<div style='display:flex;justify-content:space-between;align-items:center;'>";
    html += "<span style='font-size:16px;font-weight:600;'>Trip #" + String(tripLogger.getCurrentTripId()) + "</span>";
    html += "<span class='badge'>LOGGING ACTIVE</span>";
    html += "</div>";

    html += "<div class='stats'>";
    html += "<div class='stat-box'><div class='stat-title'>Records</div><div class='stat-val'>" + String((unsigned int)tripLogger.getRecordsLogged()) + "</div></div>";
    html += "<div class='stat-box'><div class='stat-title'>Distance</div><div class='stat-val'>" + String(tripLogger.getTripDistanceKm(), 2) + " km</div></div>";
    html += "<div class='stat-box'><div class='stat-title'>Max Speed</div><div class='stat-val'>" + String(tripLogger.getMaxSpeedKmh(), 1) + " km/h</div></div>";
    html += "<div class='stat-box'><div class='stat-title'>Peak Power</div><div class='stat-val'>" + String(tripLogger.getMaxPowerW(), 0) + " W</div></div>";
    html += "</div></div>";

    html += "<div class='card'><h3 style='margin:0 0 10px 0;'>All Trip Logs</h3><table>";
    html += "<thead><tr><th>Filename</th><th>Size</th><th>Action</th></tr></thead><tbody>";

    File root = LittleFS.open("/trips");
    if (root && root.isDirectory()) {
      std::vector<String> files;
      File f = root.openNextFile();
      while (f) {
        String name = f.path();
        if (name.endsWith(".csv")) files.push_back(name);
        f = root.openNextFile();
      }
      root.close();
      std::sort(files.begin(), files.end());

      char curPath[32];
      snprintf(curPath, sizeof(curPath), "/trips/trip_%04u.csv", tripLogger.getCurrentTripId());

      if (files.empty()) {
        html += "<tr><td colspan='3' style='color:#94a3b8;'>No trips recorded yet.</td></tr>";
      }

      for (const auto &p : files) {
        File fl = LittleFS.open(p, FILE_READ);
        size_t sz = fl ? fl.size() : 0;
        if (fl) fl.close();

        String baseName = p.substring(p.lastIndexOf('/') + 1);
        bool isCurrent = p.equals(curPath);

        html += "<tr>";
        html += "<td><strong>" + baseName + "</strong>" + (isCurrent ? " <span class='badge'>Current</span>" : "") + "</td>";
        html += "<td>" + String((float)sz / 1024.0f, 1) + " KB</td>";
        html += "<td>";
        html += "<a class='btn btn-dl' href='/download?file=" + p + "'>Download CSV</a>";
        if (!isCurrent) {
          html += "<a class='btn btn-del' href='/delete?file=" + p + "' onclick='return confirm(\"Delete " + baseName + "?\")'>Delete</a>";
        }
        html += "</td></tr>";
      }
    }
    html += "</tbody></table></div>";
    html += "<div style='font-size:12px;color:#64748b;text-align:center;'>ESP32-C3 VescBLEBridge &bull; LittleFS Flash Storage</div>";
    html += "</div></body></html>";
    server->send(200, "text/html", html);
  });

  server->on("/download", HTTP_GET, []() {
    if (!server->hasArg("file")) {
      server->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String path = server->arg("file");
    if (!LittleFS.exists(path)) {
      server->send(404, "text/plain", "File not found");
      return;
    }
    tripLogger.flush();

    File f = LittleFS.open(path, FILE_READ);
    String filename = path.substring(path.lastIndexOf('/') + 1);
    server->sendHeader("Content-Disposition", "attachment; filename=" + filename);
    server->streamFile(f, "text/csv");
    f.close();
  });

  server->on("/delete", HTTP_GET, []() {
    if (!server->hasArg("file")) {
      server->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String path = server->arg("file");
    char curPath[32];
    snprintf(curPath, sizeof(curPath), "/trips/trip_%04u.csv", tripLogger.getCurrentTripId());
    if (path.equals(curPath)) {
      server->send(400, "text/plain", "Cannot delete active trip");
      return;
    }
    LittleFS.remove(path);
    server->sendHeader("Location", "/");
    server->send(303);
  });

  server->begin();
  m_running = true;
  return true;
}

void WebPortal::stop() {
  if (!m_running) return;
  if (server) {
    server->stop();
  }
  WiFi.softAPdisconnect(true);
  m_running = false;
  Serial.println("[WiFi] Web portal stopped.");
}

void WebPortal::loop() {
  if (m_running && server) {
    server->handleClient();
  }
}
