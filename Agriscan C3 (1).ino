/*
 * AGRISCAN Sensor Reader — ESP32-C3 Compatible
 * =============================================
 * 
 * WIRING:
 * 
 *   Soil Sensor #1 (SHALLOW):
 *     VCC  → 3.3V
 *     GND  → GND
 *     AOUT → GPIO0
 * 
 *   Soil Sensor #2 (DEEP):
 *     VCC  → 3.3V
 *     GND  → GND
 *     AOUT → GPIO1
 * 
 *   DS18B20 Temperature Probe:
 *     VCC (red)     → 3.3V
 *     GND (black)   → GND
 *     DATA (yellow)  → GPIO3
 *     4.7kΩ resistor between DATA and 3.3V
 * 
 *   DHT22:
 *     VCC  → 3.3V
 *     GND  → GND
 *     DATA → GPIO5
 *     4.7kΩ resistor between DATA and 3.3V
 *     (change from your 10kΩ — 4.7kΩ is more reliable on 3.3V)
 * 
 * LIBRARIES TO INSTALL (Arduino Library Manager):
 *   1. "OneWireNg" by Piotr Stolarz
 *   2. "DallasTemperature" by Miles Burton
 *   3. "DHT sensor library for ESPx" by beegee_tokyo
 * 
 * REMOVE / DO NOT USE:
 *   - The old "OneWire" library by Paul Stoffregen (conflicts with OneWireNg)
 *   - The "Adafruit DHT" library (timing issues on C3)
 */

#include <OneWireNg_CurrentPlatform.h>  // C3-compatible OneWire
#include <drivers/DSTherm.h>            // Native DS18B20 driver (part of OneWireNg)
#include <utils/Placeholder.h>          // Memory helper for OneWireNg
#include <DHTesp.h>                     // C3-compatible DHT library

// ── Pin Definitions ──
#define SOIL_SHALLOW_PIN  0   // Analog
#define SOIL_DEEP_PIN     1   // Analog
#define DS18B20_PIN       3   // OneWire digital
#define DHT22_PIN         5   // DHT digital

// ── Sensor Objects ──
static Placeholder<OneWireNg_CurrentPlatform> owBus;
static DHTesp dht;
static bool ds18b20Found = false;

// ── Reading Interval ──
#define READ_INTERVAL_MS 2000

int readingCount = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);  // Give C3 time to stabilize serial
  
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  AGRISCAN Sensor Reader (ESP32-C3)");
  Serial.println("==========================================");
  Serial.println();

  // ── Init DS18B20 ──
  Serial.print("[DS18B20] Initializing on GPIO");
  Serial.print(DS18B20_PIN);
  Serial.println("...");
  
  new (&owBus) OneWireNg_CurrentPlatform(DS18B20_PIN, false);  // false = external pull-up
  DSTherm drv(owBus);
  
  // Search for devices on the bus
  OneWireNg::Id sensorId;
  OneWireNg::ErrorCode ec = owBus->search(sensorId);
  
  if (ec == OneWireNg::EC_DONE || ec == OneWireNg::EC_MORE) {
    ds18b20Found = true;
    Serial.print("[DS18B20] Found device ✓ Address: ");
    for (int i = 0; i < 8; i++) {
      if (sensorId[i] < 16) Serial.print("0");
      Serial.print(sensorId[i], HEX);
    }
    Serial.println();
    
    // Set 12-bit resolution
    drv.writeScratchpad(sensorId, 0, 0, DSTherm::RES_12_BIT);
  } else {
    Serial.println("[DS18B20] *** NO DEVICES FOUND ***");
    Serial.println("  → Check: yellow wire to GPIO3");
    Serial.println("  → Check: 4.7kΩ between DATA and 3.3V");
    Serial.println("  → Check: red=3.3V, black=GND");
    Serial.println("  → Note: will keep retrying each cycle");
  }
  owBus->searchReset();
  
  Serial.println();

  // ── Init DHT22 ──
  Serial.print("[DHT22] Initializing on GPIO");
  Serial.print(DHT22_PIN);
  Serial.println("...");
  
  dht.setup(DHT22_PIN, DHTesp::DHT22);
  
  // DHT22 needs ~2s warmup before first read
  Serial.print("[DHT22] Warming up");
  for (int i = 0; i < 3; i++) {
    delay(700);
    Serial.print(".");
  }
  Serial.println(" ready ✓");
  Serial.println();

  // ── Init Soil Sensors ──
  analogReadResolution(12);  // 0-4095 range
  
  int soil1 = analogRead(SOIL_SHALLOW_PIN);
  int soil2 = analogRead(SOIL_DEEP_PIN);
  Serial.print("[Soil Shallow] GPIO0 read: ");
  Serial.print(soil1);
  Serial.println(soil1 > 0 ? " ✓" : " ← CHECK WIRING");
  
  Serial.print("[Soil Deep]    GPIO1 read: ");
  Serial.print(soil2);
  Serial.println(soil2 > 0 ? " ✓" : " ← CHECK WIRING");
  
  Serial.println();
  Serial.println("Starting readings every 2 seconds...");
  Serial.println("Dip sensors in air → water → soil and watch values change.");
  Serial.println();
  Serial.println("  #  | Soil1(S) | Soil2(D) | SoilTemp | AirTemp | Humid | Notes");
  Serial.println("-----+----------+----------+----------+---------+-------+------");
}

void loop() {
  readingCount++;
  String notes = "";

  // ── Read Soil Sensors ──
  int soil1 = analogRead(SOIL_SHALLOW_PIN);
  int soil2 = analogRead(SOIL_DEEP_PIN);

  // ── Read DS18B20 ──
  float soilTemp = -999.0;
  
  DSTherm drv(owBus);
  drv.convertTempAll(DSTherm::SCAN_BUS, false);  // Start conversion on all devices
  delay(750);  // 12-bit conversion takes ~750ms
  
  // Search for first device and read it
  owBus->searchReset();
  OneWireNg::Id sensorId;
  OneWireNg::ErrorCode ec = owBus->search(sensorId);
  
  if (ec == OneWireNg::EC_DONE || ec == OneWireNg::EC_MORE) {
    Placeholder<DSTherm::Scratchpad> scratchpad;
    if (drv.readScratchpad(sensorId, scratchpad) == OneWireNg::EC_SUCCESS) {
      long rawTemp = scratchpad->getTemp();
      float tempC = (float)rawTemp / 1000.0;
      if (tempC > -50.0 && tempC < 85.0) {
        soilTemp = tempC;
      }
    }
  }
  owBus->searchReset();

  // ── Read DHT22 ──
  float airTemp = -999.0;
  float humidity = -999.0;
  
  TempAndHumidity dhtData = dht.getTempAndHumidity();
  
  if (dht.getStatus() == DHTesp::ERROR_NONE) {
    airTemp = dhtData.temperature;
    humidity = dhtData.humidity;
  }

  // ── Build Notes ──
  if (soil1 > 900 || soil2 > 900) notes += "SATURATED ";
  if (soil1 < 300 || soil2 < 300) notes += "DRY ";
  if (soilTemp == -999.0) notes += "NO_DS18B20 ";
  if (airTemp == -999.0) notes += "NO_DHT22 ";

  // ── Print Row ──
  char buf[120];
  snprintf(buf, sizeof(buf), "%4d |   %4d   |   %4d   |",
           readingCount, soil1, soil2);
  Serial.print(buf);

  // Soil temp
  if (soilTemp != -999.0) {
    Serial.print("  ");
    Serial.print(soilTemp, 1);
    Serial.print("  |");
  } else {
    Serial.print("  --.-   |");
  }

  // Air temp
  if (airTemp != -999.0) {
    Serial.print("  ");
    Serial.print(airTemp, 1);
    Serial.print("  |");
  } else {
    Serial.print("  --.-  |");
  }

  // Humidity
  if (humidity != -999.0) {
    Serial.print(" ");
    Serial.print(humidity, 0);
    Serial.print("  |");
  } else {
    Serial.print("  --  |");
  }

  Serial.print(" ");
  Serial.println(notes);

  delay(READ_INTERVAL_MS);
}
