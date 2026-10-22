#include "WebPortal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>
#include "TripLogger.h"
#include "config.h"

static WebServer *server = nullptr;
WebPortal webPortal;

WebPortal::WebPortal() : m_running(false), m_dnsServer(nullptr), m_startTime(0), m_lastStationActiveTime(0), m_hasHadClient(false) {}

bool WebPortal::start() {
  if (m_running) return true;

  Serial.println("\n[WiFi] Configuring Access Point: " WIFI_AP_SSID);

  static bool eventsRegistered = false;
  if (!eventsRegistered) {
    eventsRegistered = true;
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        Serial.printf("[WiFi] Device joined AP! MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                      info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                      info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
      } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        Serial.printf("[WiFi] Device left AP. MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                      info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                      info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
      }
    });
  }

  WiFi.disconnect(true);
  delay(50);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);

  bool ok = false;
  if (strlen(WIFI_AP_PASSWORD) >= 8) {
    ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 6, 0, 4);
  } else {
    ok = WiFi.softAP(WIFI_AP_SSID, NULL, 6, 0, 4);
  }

  if (!ok) {
    Serial.println("[WiFi] ERROR: Failed to start softAP!");
    return false;
  }

  IPAddress ip = WiFi.softAPIP();

  // Start captive portal DNS server (port 53, redirect all domains to local IP)
  if (!m_dnsServer) {
    m_dnsServer = new DNSServer();
  }
  m_dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  m_dnsServer->start(53, "*", apIP);

  Serial.printf("[WiFi] Access Point started: '%s' (IP: %s)\n", WIFI_AP_SSID, ip.toString().c_str());
  Serial.printf("[WiFi] Captive portal active at http://%s\n\n", ip.toString().c_str());

  if (!server) {
    server = new WebServer(80);
  }

  auto redirectRoot = []() {
    server->sendHeader("Location", "http://192.168.4.1/");
    server->send(302, "text/plain", "");
  };

  // Main Dashboard
  server->on("/", HTTP_GET, []() {
    String html;
    html.reserve(7168);
    html += "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>";
    html += "<title>Habaner0</title><style>";
    html += "*{-webkit-box-sizing:border-box;box-sizing:border-box;margin:0;padding:0}";
    html += "body{font-family:-apple-system,BlinkMacSystemFont,'SF Pro Text','Segoe UI',Roboto,sans-serif;";
    html += "background:#090a0b;color:#eceef0;padding:20px 16px;min-height:100vh;-webkit-font-smoothing:antialiased}";
    html += ".container{max-width:620px;margin:0 auto}";
    html += ".header{display:flex;justify-content:space-between;align-items:center;padding-bottom:16px;border-bottom:1px solid #1e2022;margin-bottom:18px}";
    html += ".brand{display:flex;align-items:center;gap:10px}";
    html += ".brand h1{font-size:17px;font-weight:700;letter-spacing:0.8px;color:#fff}";
    html += ".tag{font-size:11px;font-weight:600;letter-spacing:0.4px;color:#71767b;background:#141618;border:1px solid #222528;padding:2px 8px;border-radius:4px}";
    html += ".status-dot{width:7px;height:7px;border-radius:50%;background:#22c55e;display:inline-block;margin-right:5px}";
    html += ".live-indicator{display:inline-flex;align-items:center;font-size:12px;color:#22c55e;font-weight:500}";
    html += ".grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-bottom:20px}";
    html += ".card{background:#121416;border:1px solid #1e2124;border-radius:8px;padding:12px 14px}";
    html += ".card-label{font-size:11px;font-weight:500;color:#6b7280;text-transform:uppercase;letter-spacing:0.6px;margin-bottom:4px}";
    html += ".card-val{font-size:17px;font-weight:600;font-variant-numeric:tabular-nums;color:#fff}";
    html += ".section-title{font-size:12px;font-weight:600;color:#71767b;text-transform:uppercase;letter-spacing:0.6px;margin-bottom:10px}";
    html += ".trip-list{background:#121416;border:1px solid #1e2124;border-radius:8px;overflow:hidden;margin-bottom:24px}";
    html += ".trip-item{display:flex;justify-content:space-between;align-items:center;padding:12px 14px;border-bottom:1px solid #1a1d20}";
    html += ".trip-item:last-child{border-bottom:none}";
    html += ".trip-info{display:flex;flex-direction:column;gap:3px}";
    html += ".trip-name{font-size:14px;font-weight:600;color:#fff;font-variant-numeric:tabular-nums}";
    html += ".trip-meta{font-size:12px;color:#6b7280}";
    html += ".trip-actions{display:flex;align-items:center;gap:6px}";
    html += ".btn{display:inline-flex;align-items:center;justify-content:center;padding:6px 12px;font-size:12px;font-weight:600;border-radius:6px;text-decoration:none;cursor:pointer;border:none;transition:background 0.15s}";
    html += ".btn-view{background:#152233;color:#38bdf8;border:1px solid #1e3a5f}.btn-view:hover{background:#1e324a}";
    html += ".btn-dl{background:#1f2327;color:#fff;border:1px solid #2d3237}.btn-dl:hover{background:#2a2e34}";
    html += ".btn-del{color:#71767b;font-size:12px;padding:6px 8px;text-decoration:none}.btn-del:hover{color:#ef4444}";
    html += ".badge-cur{display:inline-block;font-size:10px;color:#22c55e;background:#14291e;border:1px solid #1b452f;padding:1px 6px;border-radius:3px;margin-left:6px}";
    html += ".footer{text-align:center;font-size:11px;color:#4b5157;margin-top:12px}";
    
    // Modal Styles
    html += ".modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.85);z-index:100;align-items:center;justify-content:center;padding:16px}";
    html += ".modal.open{display:flex}";
    html += ".modal-box{background:#121416;border:1px solid #222528;border-radius:10px;width:100%;max-width:560px;max-height:92vh;overflow-y:auto;padding:18px}";
    html += ".modal-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:14px;border-bottom:1px solid #1e2124;padding-bottom:10px}";
    html += ".modal-title{font-size:15px;font-weight:700;color:#fff}";
    html += ".close-btn{background:none;border:none;color:#71767b;font-size:22px;cursor:pointer;line-height:1;padding:4px}";
    html += ".close-btn:hover{color:#fff}";
    html += ".chart-box{background:#090a0b;border:1px solid #1e2124;border-radius:6px;padding:12px;margin-top:14px}";
    html += ".chart-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}";
    html += ".chart-title{font-size:11px;font-weight:600;color:#71767b;text-transform:uppercase;letter-spacing:0.5px}";
    html += ".legend{display:flex;gap:12px;font-size:11px;font-weight:500}";
    html += ".leg-speed{color:#38bdf8}.leg-power{color:#f59e0b}";
    html += "</style></head><body><div class='container'>";
    
    html += "<div class='header'>";
    html += "<div class='brand'>";
    html += "<h1>HABANER0</h1>";
    html += "<span class='tag'>VESC BRIDGE</span>";
    html += "</div>";
    html += "<div class='live-indicator'><span class='status-dot'></span>RECORDING</div>";
    html += "</div>";

    html += "<div class='grid'>";
    html += "<div class='card'><div class='card-label'>Current Trip</div><div class='card-val'>#" + String(tripLogger.getCurrentTripId()) + "</div></div>";
    html += "<div class='card'><div class='card-label'>Distance</div><div class='card-val'>" + String(tripLogger.getTripDistanceKm(), 2) + " km</div></div>";
    html += "<div class='card'><div class='card-label'>Max Speed</div><div class='card-val'>" + String(tripLogger.getMaxSpeedKmh(), 1) + " km/h</div></div>";
    html += "<div class='card'><div class='card-label'>Free Flash</div><div class='card-val'>" + String((unsigned int)(tripLogger.getFsFreeBytes() / 1024)) + " KB</div></div>";
    html += "</div>";

    html += "<div class='section-title'>Recorded Trips</div>";
    html += "<div class='trip-list'>";

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
      std::sort(files.begin(), files.end(), std::greater<String>());

      char curPath[32];
      snprintf(curPath, sizeof(curPath), "/trips/trip_%04u.csv", tripLogger.getCurrentTripId());

      if (files.empty()) {
        html += "<div style='padding:18px;text-align:center;font-size:13px;color:#6b7280'>No recorded trips yet.</div>";
      }

      for (const auto &p : files) {
        File fl = LittleFS.open(p, FILE_READ);
        size_t sz = fl ? fl.size() : 0;
        if (fl) fl.close();

        String baseName = p.substring(p.lastIndexOf('/') + 1);
        bool isCurrent = p.equals(curPath);

        html += "<div class='trip-item'>";
        html += "<div class='trip-info'>";
        html += "<div class='trip-name'>" + baseName + (isCurrent ? "<span class='badge-cur'>Active</span>" : "") + "</div>";
        html += "<div class='trip-meta'>" + String((float)sz / 1024.0f, 1) + " KB CSV</div>";
        html += "</div>";
        html += "<div class='trip-actions'>";
        html += "<button class='btn btn-view' onclick=\"inspectTrip('" + p + "')\">View</button>";
        html += "<a class='btn btn-dl' href='/download?file=" + p + "'>CSV</a>";
        if (!isCurrent) {
          html += "<a class='btn-del' href='/delete?file=" + p + "' onclick=\"return confirm('Delete " + baseName + "?')\">✕</a>";
        }
        html += "</div>";
        html += "</div>";
      }
    }
    html += "</div>";

    html += "<div class='footer'>Loop Recording &bull; 6384 120KV Friction &bull; 700c</div>";
    
    // Modal HTML
    html += "<div id='modal' class='modal' onclick='if(event.target===this)closeModal()'>";
    html += "<div class='modal-box'>";
    html += "<div class='modal-head'>";
    html += "<div id='mTitle' class='modal-title'>Trip Details</div>";
    html += "<button class='close-btn' onclick='closeModal()'>&times;</button>";
    html += "</div>";
    html += "<div id='mStats' class='grid' style='margin-bottom:0'></div>";
    html += "<div id='mChart' class='chart-box' style='display:none'>";
    html += "<div class='chart-head'>";
    html += "<div class='chart-title'>Ride Profile</div>";
    html += "<div class='legend'><span class='leg-speed'>&bull; Speed (km/h)</span><span class='leg-power'>&bull; Power (W)</span></div>";
    html += "</div>";
    html += "<div id='mSvg' style='height:140px'></div>";
    html += "</div>";
    html += "</div></div>";

    // Client-side Analytics JavaScript
    html += "<script>";
    html += "async function inspectTrip(path){";
    html += "document.getElementById('modal').classList.add('open');";
    html += "document.getElementById('mTitle').textContent=path.split('/').pop();";
    html += "document.getElementById('mStats').innerHTML='<div style=\"grid-column:span 2;text-align:center;color:#6b7280;padding:20px\">Analyzing trip...</div>';";
    html += "document.getElementById('mChart').style.display='none';";
    html += "try{";
    html += "const res=await fetch('/download?file='+encodeURIComponent(path));";
    html += "const text=await res.text();";
    html += "const lines=text.trim().split('\\n');";
    html += "if(lines.length<2){";
    html += "document.getElementById('mStats').innerHTML='<div style=\"grid-column:span 2;text-align:center;color:#6b7280;padding:20px\">No telemetry records logged yet.</div>';";
    html += "return;}";
    html += "const pts=[];";
    html += "for(let i=1;i<lines.length;i++){";
    html += "const p=lines[i].split(',');";
    html += "if(p.length>=7){";
    html += "pts.push({t:parseFloat(p[0]),v:parseFloat(p[1]),p:parseFloat(p[3]),s:parseFloat(p[5]),d:parseFloat(p[6]),wh:parseFloat(p[8])});";
    html += "}}";
    html += "if(!pts.length)return;";
    html += "const first=pts[0],last=pts[pts.length-1];";
    html += "const durSec=Math.max(0,last.t-first.t);";
    html += "const distKm=Math.max(0,last.d-first.d);";
    html += "let maxSpeed=0,spdSum=0,movCount=0,maxPower=0;";
    html += "pts.forEach(pt=>{";
    html += "if(pt.s>maxSpeed)maxSpeed=pt.s;";
    html += "if(pt.p>maxPower)maxPower=pt.p;";
    html += "if(pt.s>1.0){spdSum+=pt.s;movCount++;}";
    html += "});";
    html += "const avgSpeed=movCount>0?(spdSum/movCount):0;";
    html += "const energyWh=Math.max(0,last.wh-first.wh);";
    html += "const whPerKm=distKm>0.05?(energyWh/distKm):0;";
    html += "const m=Math.floor(durSec/60),s=Math.floor(durSec%60);";
    html += "const durStr=(m>0?m+'m ':'')+s+'s';";
    html += "document.getElementById('mStats').innerHTML=`";
    html += "<div class='card'><div class='card-label'>Duration</div><div class='card-val'>${durStr}</div></div>";
    html += "<div class='card'><div class='card-label'>Distance</div><div class='card-val'>${distKm.toFixed(2)} km</div></div>";
    html += "<div class='card'><div class='card-label'>Avg Speed</div><div class='card-val'>${avgSpeed.toFixed(1)} km/h</div></div>";
    html += "<div class='card'><div class='card-label'>Top Speed</div><div class='card-val'>${maxSpeed.toFixed(1)} km/h</div></div>";
    html += "<div class='card'><div class='card-label'>Energy Used</div><div class='card-val'>${energyWh.toFixed(1)} Wh</div></div>";
    html += "<div class='card'><div class='card-label'>Efficiency</div><div class='card-val'>${whPerKm?whPerKm.toFixed(1)+' Wh/km':'--'}</div></div>";
    html += "<div class='card'><div class='card-label'>Peak Power</div><div class='card-val'>${Math.round(maxPower)} W</div></div>";
    html += "<div class='card'><div class='card-label'>Voltage</div><div class='card-val'>${first.v.toFixed(1)}V &rarr; ${last.v.toFixed(1)}V</div></div>`;";
    html += "if(pts.length>1){";
    html += "document.getElementById('mChart').style.display='block';";
    html += "const W=500,H=120,pad=8;";
    html += "const tSpan=Math.max(1,last.t-first.t);";
    html += "const sMax=Math.max(15,maxSpeed*1.15);";
    html += "const pMax=Math.max(100,maxPower*1.15);";
    html += "let spPath='',pwPath='';";
    html += "pts.forEach((pt,idx)=>{";
    html += "const x=pad+((pt.t-first.t)/tSpan)*(W-pad*2);";
    html += "const yS=H-pad-(pt.s/sMax)*(H-pad*2);";
    html += "const yP=H-pad-(Math.max(0,pt.p)/pMax)*(H-pad*2);";
    html += "spPath+=(idx===0?'M':'L')+x.toFixed(1)+','+yS.toFixed(1)+' ';";
    html += "pwPath+=(idx===0?'M':'L')+x.toFixed(1)+','+yP.toFixed(1)+' ';";
    html += "});";
    html += "document.getElementById('mSvg').innerHTML=`<svg viewBox='0 0 ${W} ${H}' preserveAspectRatio='none' style='width:100%;height:100%'>";
    html += "<line x1='${pad}' y1='${H-pad}' x2='${W-pad}' y2='${H-pad}' stroke='#222528' stroke-width='1'/>";
    html += "<path d='${pwPath}' fill='none' stroke='#f59e0b' stroke-width='1.5' opacity='0.75'/>";
    html += "<path d='${spPath}' fill='none' stroke='#38bdf8' stroke-width='2'/>";
    html += "</svg>`;";
    html += "}}catch(e){document.getElementById('mStats').innerHTML='<div style=\"grid-column:span 2;color:#ef4444;padding:16px\">Failed to load log.</div>';}";
    html += "}";
    html += "function closeModal(){document.getElementById('modal').classList.remove('open');}";
    html += "</script>";

    html += "</div></body></html>";
    server->send(200, "text/html", html);
  });

  // Download handler
  server->on("/download", HTTP_GET, []() {
    if (!server->hasArg("file")) {
      server->send(400, "text/plain", "Missing file argument");
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

  // Delete handler
  server->on("/delete", HTTP_GET, []() {
    if (!server->hasArg("file")) {
      server->send(400, "text/plain", "Missing file argument");
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

  // Captive Portal Probe Endpoints (Apple, Android, Windows, Firefox)
  server->on("/hotspot-detect.html", HTTP_GET, redirectRoot);
  server->on("/canonical.html", HTTP_GET, redirectRoot);
  server->on("/generate_204", HTTP_GET, redirectRoot);
  server->on("/gen_204", HTTP_GET, redirectRoot);
  server->on("/ncsi.txt", HTTP_GET, redirectRoot);
  server->on("/connecttest.txt", HTTP_GET, redirectRoot);
  server->on("/redirect", HTTP_GET, redirectRoot);

  server->onNotFound(redirectRoot);

  server->begin();
  m_running = true;
  m_startTime = millis();
  m_lastStationActiveTime = m_startTime;
  m_hasHadClient = false;
  return true;
}

void WebPortal::stop() {
  if (!m_running) return;
  if (server) {
    server->stop();
  }
  if (m_dnsServer) {
    m_dnsServer->stop();
    delete m_dnsServer;
    m_dnsServer = nullptr;
  }
  WiFi.softAPdisconnect(true);
  m_running = false;
  Serial.println("[WiFi] Web portal and captive portal stopped.");
}

void WebPortal::loop() {
  if (!m_running) return;

  if (m_dnsServer) {
    m_dnsServer->processNextRequest();
  }
  if (server) {
    server->handleClient();
  }

  // Auto-off logic:
  // If a device connects, keep Wi-Fi ON indefinitely.
  // If no device connects within WIFI_IDLE_TIMEOUT_MS (3 mins), turn off Wi-Fi.
  uint8_t stationCount = WiFi.softAPgetStationNum();
  uint32_t now = millis();

  if (stationCount > 0) {
    m_lastStationActiveTime = now;
    m_hasHadClient = true;
  } else {
    if (!m_hasHadClient) {
      if (now - m_startTime >= WIFI_IDLE_TIMEOUT_MS) {
        Serial.println("[WiFi] 3 minutes idle with no device connected. Stopping Wi-Fi to save power.");
        stop();
      }
    } else {
      if (now - m_lastStationActiveTime >= WIFI_IDLE_TIMEOUT_MS) {
        Serial.println("[WiFi] Device disconnected and 3 minutes idle. Stopping Wi-Fi to save power.");
        stop();
      }
    }
  }
}
