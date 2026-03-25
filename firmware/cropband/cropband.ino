/**
 * AgriScan CropBand Node Firmware
 * Target: ESP32-C3
 *
 * Sensors:
 *   - DS18B20 soil temperature on GPIO3
 *   - DHT22 air temp + humidity on GPIO4 (optional, disable with DHT_ENABLED)
 *   - Capacitive soil moisture ADC on GPIO1
 *
 * Sends CropBandPacket via ESP-NOW to Hub (broadcast), then deep sleeps 1 hour.
 *
 * CropBandPacket struct MUST remain byte-for-byte identical to Hub definition.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define DHT_ENABLED false   // Set true if DHT22 is wired
#define DHT_PIN     4

#define SOIL_ADC_PIN 1      // GPIO1 — capacitive soil moisture
#define TEMP_PIN     3      // GPIO3 — DS18B20 soil temperature

#if DHT_ENABLED
#include <DHT.h>
DHT dht(DHT_PIN, DHT22);
#endif

// =============================================================================
// CropBandPacket — IDENTICAL to Hub (same field order, same __attribute__)
// =============================================================================

typedef struct __attribute__((packed)) {
  uint8_t  version;       // must be 1
  uint16_t raw_adc;       // soil moisture ADC 0-4095, or 0xFFFF if sensor absent
  float    temp_c;        // DS18B20 temp in Celsius, or -1.0 if unavailable
  float    humidity;      // DHT22 humidity %, or -1.0 if unavailable
  float    air_temp_c;    // DHT22 air temp in Celsius, or -1.0 if unavailable
  uint8_t  battery_pct;   // 0-100, or 255 if unknown
  uint32_t timestamp;     // Unix timestamp or 0 if no RTC
  uint8_t  crc8;          // CRC8 over all preceding bytes
} CropBandPacket;

// =============================================================================
// CRC8 — IDENTICAL to Hub (poly=0x07, init=0xFF)
// =============================================================================

uint8_t calcCRC8(const uint8_t *data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
  }
  return crc;
}

// =============================================================================
// ESP-NOW send callback (for debug)
// =============================================================================

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.printf("[ESPNOW] Send status: %s\n",
                status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[CROPBAND] Waking up");

  // --- DS18B20 soil temperature ---
  OneWire oneWire(TEMP_PIN);
  DallasTemperature tempSensor(&oneWire);
  tempSensor.begin();
  float temp_c = -1.0f;
  if (tempSensor.getDeviceCount() > 0) {
    tempSensor.requestTemperatures();
    float t = tempSensor.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C)
      temp_c = t;
  }
  Serial.printf("[SENSOR] soil_temp=%.2f\n", temp_c);

  // --- DHT22 air temp + humidity (optional) ---
  float humidity    = -1.0f;
  float air_temp_c  = -1.0f;
#if DHT_ENABLED
  dht.begin();
  delay(2000); // DHT22 needs ~2 s after power-up
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h)) humidity   = h;
  if (!isnan(t)) air_temp_c = t;
#endif
  Serial.printf("[SENSOR] humidity=%.1f air_temp=%.2f\n", humidity, air_temp_c);

  // --- Soil ADC ---
  // Note: on ESP32-C3, analogRead(GPIO1) should work via Arduino framework.
  // If it causes issues, switch to ESP-IDF oneshot ADC API.
  uint16_t raw_adc;
  int adc = analogRead(SOIL_ADC_PIN);
  if (adc < 200 || adc > 3900) {
    raw_adc = 0xFFFF; // sensor absent or out of range
  } else {
    raw_adc = (uint16_t)adc;
  }
  Serial.printf("[SENSOR] soil_adc=%d (raw_adc=0x%04X)\n", adc, raw_adc);

  // --- Build packet ---
  CropBandPacket pkt;
  pkt.version     = 1;
  pkt.raw_adc     = raw_adc;
  pkt.temp_c      = temp_c;
  pkt.humidity    = humidity;
  pkt.air_temp_c  = air_temp_c;
  pkt.battery_pct = 255; // unknown — no fuel gauge on this board
  pkt.timestamp   = 0;   // no RTC — Hub will use its own clock
  pkt.crc8        = calcCRC8((const uint8_t *)&pkt, sizeof(pkt) - 1);

  Serial.printf("[PKT] version=%d raw_adc=0x%04X temp_c=%.2f "
                "humidity=%.1f air_temp_c=%.2f battery_pct=%d ts=%lu crc=0x%02X\n",
                pkt.version, pkt.raw_adc, pkt.temp_c,
                pkt.humidity, pkt.air_temp_c, pkt.battery_pct,
                (unsigned long)pkt.timestamp, pkt.crc8);

  // --- ESP-NOW ---
  // WiFi must be in STA mode before esp_now_init()
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] Init failed");
    esp_deep_sleep(3600ULL * 1000000ULL);
    return;
  }
  esp_now_register_send_cb(onDataSent);

  // Broadcast peer (FF:FF:FF:FF:FF:FF)
  esp_now_peer_info_t peer = {};
  memset(peer.peer_addr, 0xFF, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  esp_now_send(peer.peer_addr, (const uint8_t *)&pkt, sizeof(pkt));

  delay(200); // allow send callback to fire before sleeping

  Serial.println("[CROPBAND] Going to sleep for 1 hour");
  esp_deep_sleep(3600ULL * 1000000ULL);
}

void loop() {
  // Never reached — deep sleep restarts from setup()
}
