#pragma once

// ============================================================================
//  VescBLEBridge - user configuration
//
//  Everything you might want to tweak lives in this file. Edit the values
//  below, then re-flash the firmware. main.cpp reads from here.
// ============================================================================

// ---------------------------------------------------------------------------
//  BLE device
// ---------------------------------------------------------------------------

// Name shown when scanning for Bluetooth devices (and in the VESC Tool app).
#define BLE_DEVICE_NAME "VescBLEBridge"

// Transmit power. One of ESP_PWR_LVL_N24 ... ESP_PWR_LVL_P9 (P9 = maximum).
#define BLE_TX_POWER ESP_PWR_LVL_P9

// Default MTU size. Negotiated higher with the client on connect if supported.
#define DEFAULT_MTU_SIZE 128

// ---------------------------------------------------------------------------
//  UART (link to the VESC)
// ---------------------------------------------------------------------------

#define VESC_UART_BAUD 115200

// UART pins. Board-specific defaults below; override if your wiring differs.
#if defined(ARDUINO_SEEED_XIAO_ESP32C6)
#define VESC_RX_PIN D10
#define VESC_TX_PIN D9
#else
// Original ESP32-C3 pins.
#define VESC_RX_PIN 20
#define VESC_TX_PIN 21
#endif

// ---------------------------------------------------------------------------
//  BLE service / characteristic UUIDs
//
//  These are the Nordic UART Service UUIDs the VESC Tool app expects, so you
//  normally should NOT change them. Generate new ones at
//  https://www.uuidgenerator.net/ only if you know what you are doing.
// ---------------------------------------------------------------------------

#define VESC_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define VESC_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define VESC_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ---------------------------------------------------------------------------
//  Optional: BLE security (passkey / "password" pairing)
//
//  By default anyone in range can connect to the bridge. Uncomment the line
//  below to require pairing with a fixed 6-digit passkey: the client (e.g.
//  your phone) is prompted for BLE_SECURITY_PASSKEY when it first connects,
//  and the bond is remembered afterwards.
//
//  Note: the connecting app must support BLE passkey entry.
// ---------------------------------------------------------------------------

// #define ENABLE_BLE_SECURITY

// The 6-digit passkey (000000 - 999999) required when ENABLE_BLE_SECURITY is
// set.
#define BLE_SECURITY_PASSKEY 123456

// ---------------------------------------------------------------------------
//  Trip Logging Configuration
// ---------------------------------------------------------------------------

// Master enable switch for automatic trip logging.
#define ENABLE_TRIP_LOGGING true

// Loop recording: Maximum number of trip files to keep on flash.
// When a new trip is created or this limit is exceeded, older trips are
// automatically deleted.
#define MAX_STORED_TRIPS 10

// Small RAM buffer size for batched flash writes (bytes).
// Keeps RAM usage tiny and protects against sudden power loss.
#define LOG_BUFFER_SIZE 256

// How frequently to log telemetry rows to LittleFS (in milliseconds).
#define LOG_INTERVAL_MS 1000

// How frequently to flush the file to flash to protect against sudden power
// loss (ms).
#define FLUSH_INTERVAL_MS 3000

// Minimum free flash space in LittleFS (bytes) before pruning the oldest trip
// files.
#define MIN_LITTLEFS_FREE_BYTES (128 * 1024)

// Vehicle speed & distance estimation parameters.
// Setup: 6384 BLDC motor (63mm diameter outrunner, 14 poles / 7 pole pairs,
// 120kv) friction-driving a 700c bike wheel (~680mm diameter with tire).
#define MOTOR_POLE_PAIRS 7 // 14 poles / 2 = 7 pole pairs for 6384 motor
#define FRICTION_ROLLER_MM                                                     \
  63.0f // 63mm outer diameter of the 6384 motor can / roller
#define WHEEL_DIAMETER_MM                                                      \
  680.0f // 700c bike wheel outer diameter with tire (~680mm)
#define GEAR_RATIO                                                             \
  (WHEEL_DIAMETER_MM / FRICTION_ROLLER_MM) // ~10.79:1 friction drive reduction

// ---------------------------------------------------------------------------
//  Wi-Fi Log Download Portal
// ---------------------------------------------------------------------------
// Automatically broadcasts 'Habaner0' for 3 minutes upon power-on.
// If your device connects, Wi-Fi stays on indefinitely until disconnected.
// If no device connects within 3 minutes, Wi-Fi automatically powers off.
#define ENABLE_WIFI_PORTAL_ON_STARTUP true
#define WIFI_AP_SSID "Habaner0l"
#define WIFI_AP_PASSWORD "12345678"           // WPA2 password (min 8 chars, or "" for open)
#define WIFI_IDLE_TIMEOUT_MS (10 * 60 * 1000) // 10 minutes timeout
