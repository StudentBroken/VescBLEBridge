#pragma once

#include <Arduino.h>

// VESC Packet Command IDs
#define COMM_GET_VALUES 4
#define COMM_GET_VALUES_SETUP 50
#define COMM_GET_VALUES_SELECTIVE 51

struct VescValues {
  float temp_fet = 0.0f;          // MOSFET temperature (°C)
  float temp_motor = 0.0f;        // Motor temperature (°C)
  float current_motor = 0.0f;     // Motor current (A)
  float current_in = 0.0f;        // Battery input current (A)
  float duty_now = 0.0f;          // Current duty cycle (0.0 - 1.0)
  int32_t rpm = 0;                // Electrical RPM (ERPM)
  float v_in = 0.0f;              // Input battery voltage (V)
  float amp_hours = 0.0f;         // Amp hours drawn (Ah)
  float amp_hours_charged = 0.0f; // Amp hours charged/regenerated (Ah)
  float watt_hours = 0.0f;        // Watt hours drawn (Wh)
  float watt_hours_charged = 0.0f;// Watt hours charged/regenerated (Wh)
  int32_t tachometer = 0;         // Motor tachometer steps
  int32_t tachometer_abs = 0;     // Absolute motor tachometer steps
  uint8_t fault_code = 0;         // VESC fault code
  
  // Computed values
  float power_w = 0.0f;           // Calculated power (V * A)
  float speed_kmh = 0.0f;         // Calculated road speed (km/h)
  float distance_km = 0.0f;       // Accumulated trip distance (km)
  bool valid = false;             // True if populated from valid packet
};

class VescPacketParser {
public:
  VescPacketParser();
  
  // Feed a byte received from VESC UART. Returns true if a complete, valid packet was parsed.
  bool processByte(uint8_t b);

  // If processByte() returned true, retrieve decoded telemetry
  bool getValues(VescValues &outValues, int polePairs, float gearRatio, float wheelDiameterMm);

  // Helper to get raw payload
  const uint8_t* getPayload() const { return m_payload; }
  uint16_t getPayloadLength() const { return m_payloadLen; }

  // Generate a COMM_GET_VALUES request packet
  static void buildGetValuesPacket(uint8_t *outBuffer, size_t &outLen);

  // CRC16-CCITT calculation
  static uint16_t crc16(const uint8_t *buf, uint32_t len);

private:
  enum State {
    STATE_WAIT_START,
    STATE_LEN_1,
    STATE_LEN_2,
    STATE_PAYLOAD,
    STATE_CRC_HIGH,
    STATE_CRC_LOW,
    STATE_WAIT_STOP
  };

  State m_state;
  uint16_t m_payloadLen;
  uint16_t m_payloadIndex;
  uint16_t m_expectedCrc;
  uint8_t m_payload[512];
  uint32_t m_lastByteTime;

  bool parseTelemetryPayload(VescValues &outValues, int polePairs, float gearRatio, float wheelDiameterMm);
};
