/**
 * CropBand Test Sketch
 * ESP32-WROOM-32U — same pin assignments as HUB main.ino
 * Reads all sensors and prints to Serial every 2 seconds.
 */

#include <Arduino.h>
#include <DHT.h>
#include <DallasTemperature.h>
#include <OneWire.h>

// =============================================================================
// PIN DEFINITIONS — identical to HUB main.ino
// =============================================================================

#define SOIL_PIN          34
#define SOIL_SENSOR_2_PIN 35
#define TEMP_PIN           4
#define DHT_PIN           16
#define BATT_PIN          32

// =============================================================================
// SENSOR OBJECTS
// =============================================================================

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
DHT dht(DHT_PIN, DHT22);

// =============================================================================
// BATTERY HELPERS — same math as HUB
// =============================================================================

float readBatteryVoltage() {
  int raw = analogRead(BATT_PIN);
  float vpin = (raw / 4095.0f) * 3.3f;
  return vpin * 3.35f;
}

int getBatteryPercent(float voltage) {
  float pct = (voltage - 3.0f) / (4.2f - 3.0f) * 100.0f;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== CropBand Test ===");

  // DS18B20
  tempSensor.begin();
  Serial.println(tempSensor.getDeviceCount() > 0
    ? "✅ DS18B20 found"
    : "❌ DS18B20 NOT found — check GPIO4 + 4.7kΩ pullup");

  // DHT22 needs ~2 s after power-up
  dht.begin();
  delay(2000);
  float h = dht.readHumidity();
  Serial.println(!isnan(h)
    ? "✅ DHT22 found"
    : "❌ DHT22 NOT found — check GPIO16 + 10kΩ pullup to 3.3V");

  // Soil sensors — quick range check
  int s1 = analogRead(SOIL_PIN);
  int s2 = analogRead(SOIL_SENSOR_2_PIN);
  Serial.println((s1 >= 200 && s1 <= 3900)
    ? "✅ Soil sensor 1 (GPIO34) in range"
    : "⚠️  Soil sensor 1 (GPIO34) out of range — Raw: " + String(s1));
  Serial.println((s2 >= 200 && s2 <= 3900)
    ? "✅ Soil sensor 2 (GPIO35) in range"
    : "⚠️  Soil sensor 2 (GPIO35) out of range — Raw: " + String(s2));

  Serial.println("--- Starting 2-second test loop ---\n");
}

// =============================================================================
// LOOP — print all readings every 2 s
// =============================================================================

void loop() {
  // Soil ADC
  int soil1 = analogRead(SOIL_PIN);
  int soil2 = analogRead(SOIL_SENSOR_2_PIN);

  // DS18B20 soil temperature
  tempSensor.requestTemperatures();
  float soilTemp = tempSensor.getTempCByIndex(0);
  bool tempOk = (soilTemp != DEVICE_DISCONNECTED_C);

  // DHT22 air temp + humidity
  float humidity = dht.readHumidity();
  float airTemp  = dht.readTemperature();

  // Battery
  float battV   = readBatteryVoltage();
  int   battPct = getBatteryPercent(battV);

  // --- Print ---
  Serial.println("────────────────────────────");
  Serial.printf("[SOIL1]  raw=%4d  %s\n", soil1,
    (soil1 >= 200 && soil1 <= 3900) ? "OK" : "OUT OF RANGE");
  Serial.printf("[SOIL2]  raw=%4d  %s\n", soil2,
    (soil2 >= 200 && soil2 <= 3900) ? "OK" : "OUT OF RANGE");

  if (tempOk)
    Serial.printf("[DS18B20] soil_temp=%.2f°C\n", soilTemp);
  else
    Serial.println("[DS18B20] NOT CONNECTED");

  if (!isnan(humidity))
    Serial.printf("[DHT22]  air_temp=%.1f°C  humidity=%.1f%%\n", airTemp, humidity);
  else
    Serial.println("[DHT22]  NOT CONNECTED");

  Serial.printf("[BATT]   %.2fV  %d%%\n", battV, battPct);

  delay(2000);
}
