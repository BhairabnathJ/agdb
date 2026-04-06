#include <OneWire.h>

// On the XIAO, it's often safer to use the D-designation 
// instead of the integer 3.
#define TEMP_PIN 3

OneWire ds(TEMP_PIN);

void setup() {
  Serial.begin(115200);
  delay(3000); // Wait for native USB serial
  Serial.println("--- 1-Wire Scanner Starting ---");
}

void loop() {
  byte i;
  byte addr[8];

  Serial.println("Scanning 1-Wire bus...");

  if (!ds.search(addr)) {
    Serial.println("No more addresses. Resetting search.");
    Serial.println("-------------------------");
    ds.reset_search();
    delay(2000);
    return;
  }

  Serial.print("ROM =");
  for (i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }
  Serial.println();

  // The first byte of a DS18B20 should be 0x28
  if (addr[0] == 0x28) {
    Serial.println("Found a DS18B20 sensor!");
  } else {
    Serial.println("Device is not a DS18B20 family device.");
  }
  
  delay(1000);
}