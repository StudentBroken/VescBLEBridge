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

//#define ENABLE_BLE_SECURITY

// The 6-digit passkey (000000 - 999999) required when ENABLE_BLE_SECURITY is set.
#define BLE_SECURITY_PASSKEY 123456

// ---------------------------------------------------------------------------
//  Trip Logging Configuration
// ---------------------------------------------------------------------------

// Master enable switch for automatic trip logging.
#define ENABLE_TRIP_LOGGING true

// Loop recording: Maximum number of trip files to keep on flash.
// When a new trip is created or this limit is exceeded, older trips are automatically deleted.
#define MAX_STORED_TRIPS 10

// Small RAM buffer size for batched flash writes (bytes).
// Keeps RAM usage tiny and protects against sudden power loss.
#define LOG_BUFFER_SIZE 256

// How frequently to log telemetry rows to LittleFS (in milliseconds).
#define LOG_INTERVAL_MS 1000

// How frequently to flush the file to flash to protect against sudden power loss (ms).
#define FLUSH_INTERVAL_MS 3000

// Minimum free flash space in LittleFS (bytes) before pruning the oldest trip files.
#define MIN_LITTLEFS_FREE_BYTES (128 * 1024)

// Vehicle speed & distance estimation parameters.
// Adjust these to match your bike or board:
#define MOTOR_POLE_PAIRS 7        // Number of motor pole pairs (e.g. 7 for standard hub/outrunner)
#define GEAR_RATIO 1.0f           // Motor revs per wheel rev (1.0 for direct-drive hub)
#define WHEEL_DIAMETER_MM 200.0f  // Wheel diameter in mm (e.g. 700c bike ~670mm, 8" scooter ~200mm)

// ---------------------------------------------------------------------------
//  Wi-Fi Log Download Portal (Optional)
// ---------------------------------------------------------------------------
// Connect to this AP from your phone or laptop and open http://192.168.4.1
// to download or delete trip CSV files.
// Can also be launched anytime by typing 'wifi' in the USB serial terminal.
#define ENABLE_WIFI_PORTAL_ON_STARTUP false
#define WIFI_AP_SSID "VescTripLogs"
#define WIFI_AP_PASSWORD ""       // Leave empty for open access
