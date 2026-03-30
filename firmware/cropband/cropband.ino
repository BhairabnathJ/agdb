#include <Arduino.h>
#include <OneWire.h>
#include <DS18B20.h>

#define TEMP_PIN 3

OneWire oneWire(TEMP_PIN);
DS18B20 tempSensor(&oneWire);

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("[TEST] DS18B20 only — GPIO 3");

  if (tempSensor.begin()) {
    Serial.println("[TEST] DS18B20 found");
  } else {
    Serial.println("[TEST] DS18B20 NOT found — check wiring");
  }
}

void loop() {
  if (tempSensor.begin()) {
    tempSensor.requestTemperatures();
    unsigned long start = millis();
    while (!tempSensor.isConversionComplete()) {
      if (millis() - start > 2000) {
        Serial.println("[TEST] Conversion timeout");
        break;
      }
    }
    float t = tempSensor.getTempC();
    Serial.printf("[TEMP] %.2f °C\n", t);
  } else {
    Serial.println("[TEMP] Sensor not detected");
  }

  delay(2000);
}