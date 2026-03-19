/**
 * AgriScan CropBand — Sensor Test Sketch
 * Board: ESP32-C3 SuperMini
 * 
 * PURPOSE: Verify sensors work on the C3 before adding ESP-NOW.
 *          Reads capacitive soil moisture sensor + DS18B20, prints to Serial.
 * 
 * WIRING:
 *   Capacitive Soil Moisture Sensor v1.2:
 *     VCC  → 3.3V
 *     GND  → GND
 *     AOUT → GPIO1 (ADC1_CH1)
 * 
 *   DS18B20 Temperature Probe:
 *     VCC (red)    → 3.3V
 *     GND (black)  → GND
 *     DATA (yellow) → GPIO3
 *     4.7kΩ resistor between DATA and 3.3V
 * 
 * BOARD SETUP in Arduino IDE:
 *   Board: "ESP32C3 Dev Module"
 *   USB CDC On Boot: "Enabled"    ← IMPORTANT or Serial Monitor won't work
 *   Flash Size: 4MB
 *   Upload Speed: 921600
 * 
 * NOTE on ADC:
 *   The C3 has a 12-bit ADC (0-4095) like the WROOM-32U, but uses a 
 *   different peripheral (SAR ADC). Default attenuation is 11dB (0-3.3V).
 *   Your calibration curve from the Hub (ADC 1100-2600) may NOT transfer 
 *   directly — that's one of the things this test will help you figure out.
 */

#include <OneWire.h>
#include <DallasTemperature.h>

// =============================================================================
// PIN DEFINITIONS — C3 SuperMini
// =============================================================================
#define SOIL_PIN  1   // GPIO1 — ADC1_CH1, analog read
#define TEMP_PIN  3   // GPIO3 — OneWire for DS18B20

// =============================================================================
// GLOBALS
// =============================================================================
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  
  // Give USB CDC time to connect
  delay(2000);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("  AgriScan CropBand — Sensor Test");
  Serial.println("  Board: ESP32-C3 SuperMini");
  Serial.println("========================================");
  Serial.println();

  // Init temperature sensor
  tempSensor.begin();
  int deviceCount = tempSensor.getDeviceCount();
  Serial.printf("[TEMP] DS18B20 devices found: %d\n", deviceCount);
  
  if (deviceCount == 0) {
    Serial.println("[TEMP] ⚠ No DS18B20 found! Check wiring:");
    Serial.println("       - DATA pin → GPIO3");
    Serial.println("       - 4.7kΩ pull-up between DATA and 3.3V");
    Serial.println("       - VCC → 3.3V, GND → GND");
  }

  // Quick ADC sanity check
  int testRead = analogRead(SOIL_PIN);
  Serial.printf("[SOIL] Initial ADC read: %d\n", testRead);
  
  if (testRead == 0) {
    Serial.println("[SOIL] ⚠ ADC reading is 0. Check wiring:");
    Serial.println("       - AOUT → GPIO1");
    Serial.println("       - VCC → 3.3V, GND → GND");
  } else if (testRead >= 4095) {
    Serial.println("[SOIL] ⚠ ADC is maxed at 4095. Sensor may be in air");
    Serial.println("       or wiring issue. This is normal if dry/in air.");
  }

  Serial.println();
  Serial.println("[READY] Starting sensor loop — reading every 2 seconds");
  Serial.println("        Put sensor in air, then water, then soil.");
  Serial.println("        Watch ADC values change.");
  Serial.println();
  Serial.println("  ADC high (~2400-2800) = DRY (air)");
  Serial.println("  ADC low  (~1000-1400) = WET (water)");
  Serial.println("  (These ranges are approximate — your C3 may differ)");
  Serial.println();
  Serial.println("  #  |  ADC Raw  |  Temp °C  |  Note");
  Serial.println("-----+-----------+-----------+--------");
}

// =============================================================================
// LOOP
// =============================================================================
int readingCount = 0;

void loop() {
  readingCount++;
  
  // --- Read soil moisture ADC ---
  // Take 10 samples and average to reduce noise
  long adcSum = 0;
  for (int i = 0; i < 10; i++) {
    adcSum += analogRead(SOIL_PIN);
    delay(10);
  }
  int adcAvg = adcSum / 10;

  // --- Read DS18B20 temperature ---
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);
  
  // -127 means DS18B20 read failed
  bool tempValid = (tempC > -50.0 && tempC < 80.0);

  // --- Classify for quick visual feedback ---
  const char* note = "";
  if (adcAvg > 2600)      note = "VERY DRY / AIR";
  else if (adcAvg > 2000) note = "DRY";
  else if (adcAvg > 1500) note = "MOIST";
  else if (adcAvg > 1100) note = "WET";
  else                     note = "SATURATED / WATER";

  // --- Print ---
  Serial.printf("%4d |  %4d     |  ", readingCount, adcAvg);
  
  if (tempValid) {
    Serial.printf("%.1f°C   |  %s", tempC, note);
  } else {
    Serial.printf(" --.-    |  %s  [TEMP FAIL]", note);
  }
  Serial.println();

  delay(2000);  // 2 second loop, same as Hub
}
