/**
 * CropBand Firmware — ESP32-WROOM-38U
 * Reads two soil sensors, DS18B20, DHT22, and battery.
 * Packs into CropBandPacket and sends via ESP-NOW every 25 seconds.
 *
 * Hub MAC: uses broadcast (FF:FF:FF:FF:FF:FF) — Hub receives regardless of its
 * own MAC.  No need to hardcode the Hub MAC at all.  If you later want unicast
 * (lower RF overhead), flash the Hub, open Serial Monitor, copy the MAC printed
 * at boot ("[BOOT] WiFi: AgriScan_Connect"), paste it into HUB_MAC below, and
 * switch BROADCAST_MODE to false.
 */

#include <Arduino.h>
#include <DHT.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFi.h>
#include <esp_now.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define BROADCAST_MODE true  // false = use HUB_MAC unicast

// Replace with Hub MAC if BROADCAST_MODE is false.
// To find it: flash Hub, open Serial Monitor, look for the MAC printed at boot.
static uint8_t HUB_MAC[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

#define SEND_INTERVAL_MS 25000

// =============================================================================
// PIN DEFINITIONS — identical to HUB main.ino
// =============================================================================

#define SOIL_PIN          34   // sensor 1, shallow
#define SOIL_SENSOR_2_PIN 35   // sensor 2, deep
#define TEMP_PIN           4   // DS18B20 data
#define DHT_PIN           16   // DHT22 data
#define BATT_PIN          32   // battery voltage divider (ADC1)

// =============================================================================
// PACKET STRUCT — must match Hub main.ino byte-for-byte
// =============================================================================

typedef struct __attribute__((packed)) {
  uint8_t  version;     // must be 1
  uint16_t raw_adc;     // soil sensor 1 (shallow) ADC 0-4095, or 0xFFFF if absent
  uint16_t raw_adc_2;   // soil sensor 2 (deep) ADC 0-4095, or 0xFFFF if absent
  float    temp_c;      // DS18B20 temp in Celsius, or -1.0 if unavailable
  float    humidity;    // DHT22 humidity %, or -1.0 if unavailable
  float    air_temp_c;  // DHT22 air temp in Celsius, or -1.0 if unavailable
  uint8_t  battery_pct; // 0-100, or 255 if unknown
  uint32_t timestamp;   // 0 — Hub uses its own clock
  uint8_t  crc8;        // CRC8 over all preceding bytes (sizeof-1)
} CropBandPacket;

// =============================================================================
// SENSOR OBJECTS
// =============================================================================

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
DHT dht(DHT_PIN, DHT22);

// =============================================================================
// CRC8 — polynomial 0x07, init 0xFF (must match Hub exactly)
// =============================================================================

static uint8_t calcCRC8(const uint8_t *data, size_t len) {
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
// BATTERY HELPERS — same math as HUB
// =============================================================================

static float readBatteryVoltage() {
  int raw = analogRead(BATT_PIN);
  float vpin = (raw / 4095.0f) * 3.3f;
  return vpin * 3.35f;
}

static int getBatteryPercent(float voltage) {
  float pct = (voltage - 3.0f) / (4.2f - 3.0f) * 100.0f;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

// =============================================================================
// ESP-NOW SEND CALLBACK
// =============================================================================

static volatile bool sendDone = false;
static volatile esp_now_send_status_t sendStatus;

void onSendComplete(const uint8_t *mac, esp_now_send_status_t status) {
  sendStatus = status;
  sendDone = true;
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== CropBand Boot ===");

  // DS18B20
  tempSensor.begin();
  Serial.println(tempSensor.getDeviceCount() > 0
    ? "  DS18B20 OK"
    : "  DS18B20 NOT FOUND — check GPIO4 + 4.7kΩ pullup");

  // DHT22 needs ~2 s after power-up
  dht.begin();
  delay(2000);
  float h = dht.readHumidity();
  Serial.println(!isnan(h)
    ? "  DHT22 OK"
    : "  DHT22 NOT FOUND — check GPIO16 + 10kΩ pullup to 3.3V");

  int s1 = analogRead(SOIL_PIN);
  int s2 = analogRead(SOIL_SENSOR_2_PIN);
  Serial.printf("  Soil1 GPIO34 raw=%d  %s\n", s1,
    (s1 >= 200 && s1 <= 3900) ? "OK" : "OUT OF RANGE");
  Serial.printf("  Soil2 GPIO35 raw=%d  %s\n", s2,
    (s2 >= 200 && s2 <= 3900) ? "OK" : "OUT OF RANGE");

  // WiFi in STA mode — required for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("  CropBand MAC: ");
  Serial.println(WiFi.macAddress());

  // ESP-NOW init
  if (esp_now_init() != ESP_OK) {
    Serial.println("  ESP-NOW init FAILED — halting");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(onSendComplete);

  // Register peer
  esp_now_peer_info_t peer = {};
  peer.channel = 1;   // must match Hub's WiFi channel (softAP default is 1)
  peer.encrypt = false;

#if BROADCAST_MODE
  memset(peer.peer_addr, 0xFF, 6);
  Serial.println("  ESP-NOW peer: BROADCAST (FF:FF:FF:FF:FF:FF)");
#else
  memcpy(peer.peer_addr, HUB_MAC, 6);
  Serial.printf("  ESP-NOW peer: %02X:%02X:%02X:%02X:%02X:%02X (unicast)\n",
    HUB_MAC[0], HUB_MAC[1], HUB_MAC[2], HUB_MAC[3], HUB_MAC[4], HUB_MAC[5]);
#endif

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("  ESP-NOW add_peer FAILED — halting");
    while (true) delay(1000);
  }

  Serial.println("=== CropBand Ready — sending every 25 s ===\n");
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  // --- Read sensors ---
  int soil1 = analogRead(SOIL_PIN);
  int soil2 = analogRead(SOIL_SENSOR_2_PIN);

  tempSensor.requestTemperatures();
  float soilTemp = tempSensor.getTempCByIndex(0);
  bool tempOk = (soilTemp != DEVICE_DISCONNECTED_C);
  if (!tempOk) soilTemp = -1.0f;

  float humidity = dht.readHumidity();
  float airTemp  = dht.readTemperature();
  if (isnan(humidity)) humidity = -1.0f;
  if (isnan(airTemp))  airTemp  = -1.0f;

  float battV   = readBatteryVoltage();
  int   battPct = getBatteryPercent(battV);

  // --- Build packet ---
  CropBandPacket pkt = {};
  pkt.version     = 1;
  pkt.raw_adc     = (soil1 >= 200 && soil1 <= 3900) ? (uint16_t)soil1   : 0xFFFF;
  pkt.raw_adc_2   = (soil2 >= 200 && soil2 <= 3900) ? (uint16_t)soil2   : 0xFFFF;
  pkt.temp_c      = soilTemp;
  pkt.humidity    = humidity;
  pkt.air_temp_c  = airTemp;
  pkt.battery_pct = (uint8_t)battPct;
  pkt.timestamp   = 0; // Hub uses its own clock
  pkt.crc8        = calcCRC8((const uint8_t *)&pkt, sizeof(pkt) - 1);

  // --- Print all values ---
  Serial.println("────────────────────────────────────");
  Serial.printf("[SOIL1]   raw=%4d  %s\n", soil1,
    pkt.raw_adc != 0xFFFF ? "valid" : "absent/out-of-range");
  Serial.printf("[SOIL2]   raw=%4d  %s\n", soil2,
    pkt.raw_adc_2 != 0xFFFF ? "valid" : "absent/out-of-range");
  Serial.printf("[DS18B20] soil_temp=%.2f°C  %s\n", soilTemp,
    tempOk ? "OK" : "DISCONNECTED");
  Serial.printf("[DHT22]   air_temp=%.1f°C  humidity=%.1f%%  %s\n",
    airTemp, humidity, humidity >= 0 ? "OK" : "DISCONNECTED");
  Serial.printf("[BATT]    %.2fV  %d%%\n", battV, battPct);
  Serial.printf("[PKT]     version=%d  crc8=0x%02X  size=%d bytes\n",
    pkt.version, pkt.crc8, (int)sizeof(pkt));

  // --- Send ---
  sendDone = false;

#if BROADCAST_MODE
  static uint8_t broadcastAddr[6] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
  esp_err_t result = esp_now_send(broadcastAddr, (uint8_t *)&pkt, sizeof(pkt));
#else
  esp_err_t result = esp_now_send(HUB_MAC, (uint8_t *)&pkt, sizeof(pkt));
#endif

  if (result != ESP_OK) {
    Serial.printf("[SEND]    esp_now_send FAILED (err=0x%X)\n", result);
  } else {
    // Wait up to 200 ms for the send callback
    unsigned long t = millis();
    while (!sendDone && millis() - t < 200) delay(5);
    if (sendDone) {
      Serial.printf("[SEND]    %s\n",
        sendStatus == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "DELIVERY FAILED");
    } else {
      Serial.println("[SEND]    timeout waiting for ack");
    }
  }

  Serial.println();
  delay(SEND_INTERVAL_MS);
}
