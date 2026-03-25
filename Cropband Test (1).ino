/**
 * AgriScan CropBand — Sensor Test Sketch v2
 * Board: ESP32-C3 SuperMini
 * 
 * SENSORS:
 *   - 2x Capacitive Soil Moisture Sensor v1.2
 *   - 1x DS18B20 Temperature Probe (soil temp)
 *   - 1x DHT22 (air temp + humidity)
 * 
 * WIRING:
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
 *     10kΩ resistor between DATA and 3.3V
 * 
 * ARDUINO IDE SETTINGS:
 *   Board:            "ESP32C3 Dev Module"
 *   USB CDC On Boot:  "Enabled"   ← NO OUTPUT WITHOUT THIS
 *   Flash Size:       4MB
 *   Upload Speed:     921600
 */

#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// =============================================================================
// PIN DEFINITIONS — C3 SuperMini
// =============================================================================
// ADC pins (GPIO 0-4 are ADC-capable on C3)
#define SOIL_SHALLOW_PIN  0   // GPIO0 — ADC1_CH0
#define SOIL_DEEP_PIN     1   // GPIO1 — ADC1_CH1

// Digital pins
#define DS18B20_PIN       3   // GPIO3 — OneWire
#define DHT_PIN           5   // GPIO5 — DHT22 data

#define DHT_TYPE DHT22

// =============================================================================
// GLOBALS
// =============================================================================
OneWire oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);

int readingCount = 0;

// =============================================================================
// HELPER: Average 10 ADC reads to reduce noise
// =============================================================================
int readADC(int pin) {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return sum / 10;
}

// =============================================================================
// HELPER: Classify moisture level from ADC value
// =============================================================================
const char* classifyMoisture(int adc) {
  if (adc > 2600)      return "VERY DRY / AIR";
  if (adc > 2000)      return "DRY";
  if (adc > 1500)      return "MOIST";
  if (adc > 1100)      return "WET";
  return "SATURATED";
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(3000);  // Extra time for USB CDC to connect

  Serial.println();
  Serial.println("=============================================");
  Serial.println("  AgriScan CropBand — Sensor Test v2");
  Serial.println("  Board: ESP32-C3 SuperMini");
  Serial.println("  Sensors: 2x Soil, DS18B20, DHT22");
  Serial.println("=============================================");
  Serial.println();

  // --- Init DS18B20 ---
  tempSensor.begin();
  int ds18Count = tempSensor.getDeviceCount();
  Serial.printf("[DS18B20] Devices found: %d", ds18Count);
  if (ds18Count == 0) {
    Serial.println(" ← CHECK WIRING (GPIO3 + 4.7k pull-up)");
  } else {
    Serial.println(" ← OK");
  }

  // --- Init DHT22 ---
  dht.begin();
  delay(2000);  // DHT22 needs 2s after power-on before first read
  float testHum = dht.readHumidity();
  Serial.printf("[DHT22]  ");
  if (isnan(testHum)) {
    Serial.println("No response ← CHECK WIRING (GPIO5 + 10k pull-up)");
  } else {
    Serial.printf("Humidity: %.1f%% ← OK\n", testHum);
  }

  // --- Test ADC pins ---
  int testShallow = analogRead(SOIL_SHALLOW_PIN);
  int testDeep    = analogRead(SOIL_DEEP_PIN);
  Serial.printf("[SOIL 1] Shallow initial read: %d", testShallow);
  Serial.println(testShallow == 0 ? " ← CHECK WIRING (GPIO0)" : " ← OK");
  Serial.printf("[SOIL 2] Deep initial read:    %d", testDeep);
  Serial.println(testDeep == 0 ? " ← CHECK WIRING (GPIO1)" : " ← OK");

  Serial.println();
  Serial.println("Starting readings every 2 seconds...");
  Serial.println("Dip sensors in air → water → soil and watch values change.");
  Serial.println();
  Serial.println("  #  | Soil1(S) | Soil2(D) | SoilTemp | AirTemp | Humid | Notes");
  Serial.println("-----+----------+----------+----------+---------+-------+------");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  readingCount++;

  // --- Soil moisture (2 sensors) ---
  int adcShallow = readADC(SOIL_SHALLOW_PIN);
  int adcDeep    = readADC(SOIL_DEEP_PIN);

  // --- DS18B20 soil temperature ---
  tempSensor.requestTemperatures();
  float soilTempC = tempSensor.getTempCByIndex(0);
  bool soilTempOK = (soilTempC > -50.0 && soilTempC < 80.0);

  // --- DHT22 air temp + humidity ---
  float airTempC  = dht.readTemperature();
  float humidity   = dht.readHumidity();
  bool dhtOK = !isnan(airTempC) && !isnan(humidity);

  // --- Print row ---
  Serial.printf("%4d |  %4d    |  %4d    |  ",
    readingCount, adcShallow, adcDeep);

  // Soil temp
  if (soilTempOK) {
    Serial.printf("%5.1fC  |  ", soilTempC);
  } else {
    Serial.printf(" --.-   |  ");
  }

  // Air temp
  if (dhtOK) {
    Serial.printf("%5.1fC | %4.1f%% | ", airTempC, humidity);
  } else {
    Serial.printf(" --.-  |  --  | ");
  }

  // Classification based on shallow sensor
  Serial.println(classifyMoisture(adcShallow));

  delay(2000);
}
