/*
 * XIAO ESP32-C3 Capacitive Soil Moisture Sensor
 * Pin Connection: Sensor Signal -> XIAO A0 (GPIO 2)
 */

const int sensorPin = A0; // This is D0/GPIO2 on your board
int sensorValue = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- Soil Moisture Sensor Starting ---");

  // ESP32-C3 ADC is 12-bit by default (0-4095)
  analogReadResolution(12);

  // Set attenuation to 11dB to allow reading up to ~3.1V - 3.3V
  analogSetAttenuation(ADC_11db);
}

void loop() {
  // Read the analog value
  sensorValue = analogRead(sensorPin);

  // Convert to voltage for easier troubleshooting
  float voltage = sensorValue * (3.3 / 4095.0);

  Serial.print("Raw ADC Value: ");
  Serial.print(sensorValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage);
  Serial.println("V");

  // SIMPLE CALIBRATION LOGIC:
  // Most capacitive sensors read ~3000 in dry air and ~1500 in water.
  // Uncomment the lines below once you find your specific MIN/MAX values.
  /*
  int moisturePercent = map(sensorValue, 3000, 1500, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);
  Serial.print("Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");
  */

  delay(1000); // Wait 1 second between reads
}