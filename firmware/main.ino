/**
 * AgriScan ESP32 Firmware
 *
 * Implements soil moisture monitoring with:
 * - Capacitive soil moisture sensor (ADC)
 * - DS18B20 temperature sensor
 * - SD card logging
 * - WiFi captive portal
 * - Web server for dashboard
 * - LED status indicators
 *
 * @version 1.0.0
 */

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

// =============================================================================
// PIN DEFINITIONS
// =============================================================================

// Sensors
#define SOIL_SENSOR_PIN 34 // ADC1_CH6 (12-bit: 0-4095)
#define TEMP_SENSOR_PIN 4  // OneWire

// SD Card (SPI)
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCK 18
#define SD_CS 5

// RGB LED (PWM)
#define LED_RED 25
#define LED_GREEN 26
#define LED_BLUE 27

// =============================================================================
// CONFIGURATION
// =============================================================================

#define FIRMWARE_VERSION "1.0.0"
#define READING_INTERVAL_MS 900000 // 15 minutes
#define WIFI_SSID_PREFIX "AGRISCAN_"
#define DNS_PORT 53
#define WEB_PORT 80

// NTP Configuration
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// =============================================================================
// GLOBAL OBJECTS
// =============================================================================

WebServer webServer(WEB_PORT);
DNSServer dnsServer;
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

// =============================================================================
// GLOBAL STATE
// =============================================================================

// Sensor readings
int lastRaw = 0;
float lastTemp = 0.0;
float lastTheta = 0.0;
String lastReadingTimestamp = "";
String sensorStatus = "ok";
String sdCardStatus = "ok";

// Timing
unsigned long lastReadingTime = 0;
unsigned long lastSDWrite = 0;
unsigned long bootTime = 0;

// Error tracking
int errorCount24h = 0;

// =============================================================================
// SENSOR CALIBRATION (from physics.js)
// =============================================================================

// Factory calibration curve for capacitive sensors (10-bit ADC values)
// ESP32 uses 12-bit ADC (0-4095), so we divide by 4 for compatibility
const float calPoints[][2] = {
    {250, 0.00}, // Dry air
    {450, 0.10}, // Dry soil
    {650, 0.25}, // Moist soil
    {850, 0.40}, // Wet soil
    {1000, 0.50} // Saturated
};
const int numCalPoints = 5;

/**
 * Convert raw ADC to VWC (volumetric water content)
 * Uses piecewise linear interpolation
 */
float calculateTheta(int rawAdc, float tempC) {
  // Convert 12-bit ADC (0-4095) to 10-bit equivalent (0-1023)
  float raw = rawAdc / 4.0;

  // Clamp to calibration bounds
  if (raw <= calPoints[0][0])
    return calPoints[0][1];
  if (raw >= calPoints[numCalPoints - 1][0])
    return calPoints[numCalPoints - 1][1];

  // Find interpolation segment
  for (int i = 1; i < numCalPoints; i++) {
    if (raw <= calPoints[i][0]) {
      float x0 = calPoints[i - 1][0], y0 = calPoints[i - 1][1];
      float x1 = calPoints[i][0], y1 = calPoints[i][1];
      float t = (raw - x0) / (x1 - x0);
      float theta = y0 + (y1 - y0) * t;

      // Temperature compensation (reference is 20°C)
      // Minimal effect for standard agricultural range
      // theta += 0.001 * (tempC - 20.0);  // Disabled by default

      // Clamp to physical bounds
      if (theta < 0.0)
        theta = 0.0;
      if (theta > 0.50)
        theta = 0.50;

      return theta;
    }
  }

  return calPoints[numCalPoints - 1][1];
}

/**
 * Get irrigation status based on theta and thresholds
 * Matches physics.js logic
 */
String getStatus(float theta) {
  // Default thresholds (before calibration completes)
  // These are approximate values - auto-calibration refines them
  const float theta_fc = 0.32;     // Field capacity
  const float theta_refill = 0.18; // Refill point

  if (theta >= theta_fc)
    return "FULL";
  if (theta >= theta_fc * 0.9)
    return "OPTIMAL";
  if (theta >= theta_refill)
    return "MONITOR";
  return "REFILL";
}

// =============================================================================
// TIME UTILITIES
// =============================================================================

String getISOTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // Fallback to uptime-based timestamp
    unsigned long uptime = millis() / 1000;
    char buf[30];
    snprintf(buf, sizeof(buf), "1970-01-01T%02lu:%02lu:%02luZ",
             (uptime / 3600) % 24, (uptime / 60) % 60, uptime % 60);
    return String(buf);
  }

  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

String getLogTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    unsigned long uptime = millis() / 1000;
    char buf[25];
    snprintf(buf, sizeof(buf), "0000-00-00 %02lu:%02lu:%02lu",
             (uptime / 3600) % 24, (uptime / 60) % 60, uptime % 60);
    return String(buf);
  }

  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// =============================================================================
// LOGGING FUNCTIONS
// =============================================================================

void logSystem(const char *level, const char *message) {
  File file = SD.open("/logs/system.log", FILE_APPEND);
  if (!file) {
    Serial.println("[ERROR] Failed to open system.log");
    sdCardStatus = "error";
    return;
  }

  file.print("[");
  file.print(getLogTimestamp());
  file.print("] [");
  file.print(level);
  file.print("] ");
  file.println(message);
  file.flush();
  file.close();

  lastSDWrite = millis();

  // Also print to serial
  Serial.print("[");
  Serial.print(level);
  Serial.print("] ");
  Serial.println(message);
}

void logSensorReading() {
  File file = SD.open("/logs/sensor_readings.csv", FILE_APPEND);
  if (!file) {
    logSystem("ERROR", "Failed to open sensor_readings.csv");
    sdCardStatus = "error";
    return;
  }

  // Format: timestamp,raw_adc,temp_c,theta,status,qc_valid
  file.print(lastReadingTimestamp);
  file.print(",");
  file.print(lastRaw);
  file.print(",");
  file.print(lastTemp, 1);
  file.print(",");
  file.print(lastTheta, 3);
  file.print(",");
  file.print(getStatus(lastTheta));
  file.print(",");
  file.println(sensorStatus == "ok" ? "1" : "0");
  file.flush();
  file.close();

  lastSDWrite = millis();
  sdCardStatus = "ok";
}

// =============================================================================
// SENSOR FUNCTIONS
// =============================================================================

void readSensors() {
  // Read soil moisture (12-bit ADC)
  lastRaw = analogRead(SOIL_SENSOR_PIN);

  // Read temperature (DS18B20)
  tempSensor.requestTemperatures();
  lastTemp = tempSensor.getTempCByIndex(0);

  // Check for sensor errors
  if (lastRaw < 0 || lastRaw > 4095) {
    sensorStatus = "error";
    logSystem("ERROR", "Soil sensor out of range");
    errorCount24h++;
  } else if (lastTemp < -10.0 || lastTemp > 60.0 ||
             lastTemp == DEVICE_DISCONNECTED_C) {
    sensorStatus = "error";
    logSystem("ERROR", "Temperature sensor error");
    errorCount24h++;
  } else {
    sensorStatus = "ok";
  }

  // Calculate VWC
  lastTheta = calculateTheta(lastRaw, lastTemp);
  lastReadingTimestamp = getISOTimestamp();

  char msg[100];
  snprintf(msg, sizeof(msg), "Reading: raw=%d, temp=%.1f, theta=%.3f", lastRaw,
           lastTemp, lastTheta);
  logSystem("SENSOR", msg);
}

// =============================================================================
// LED CONTROL
// =============================================================================

void setLED(int r, int g, int b) {
  analogWrite(LED_RED, r);
  analogWrite(LED_GREEN, g);
  analogWrite(LED_BLUE, b);
}

void blinkLED(int r, int g, int b, int periodMs) {
  unsigned long now = millis();
  if ((now / periodMs) % 2 == 0) {
    setLED(r, g, b);
  } else {
    setLED(0, 0, 0);
  }
}

void updateLEDPattern() {
  // Priority 1: SD card error
  if (sdCardStatus != "ok") {
    blinkLED(0, 0, 255, 1000); // Blue, 1 Hz
    return;
  }

  // Priority 2: Sensor error
  if (sensorStatus != "ok") {
    blinkLED(255, 255, 255, 500); // White, 2 Hz
    return;
  }

  // Normal operation: show moisture status
  String status = getStatus(lastTheta);
  if (status == "FULL" || status == "OPTIMAL") {
    setLED(0, 255, 0); // Solid green
  } else if (status == "MONITOR") {
    setLED(255, 255, 0); // Solid yellow
  } else {
    setLED(255, 0, 0); // Solid red (REFILL)
  }
}

// =============================================================================
// SD CARD INITIALIZATION
// =============================================================================

bool initSDCard() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS)) {
    Serial.println("[ERROR] SD card initialization failed");
    sdCardStatus = "error";
    return false;
  }

  Serial.println("[INFO] SD card initialized");
  sdCardStatus = "ok";

  // Create directories if they don't exist
  if (!SD.exists("/logs")) {
    SD.mkdir("/logs");
    Serial.println("[INFO] Created /logs directory");
  }
  if (!SD.exists("/config")) {
    SD.mkdir("/config");
    Serial.println("[INFO] Created /config directory");
  }

  // Create default config if not exists
  if (!SD.exists("/config/user_prefs.json")) {
    File f = SD.open("/config/user_prefs.json", FILE_WRITE);
    if (f) {
      f.println("{\"onboarding_complete\":false}");
      f.close();
      Serial.println("[INFO] Created default user_prefs.json");
    }
  }

  // Create CSV headers if files don't exist
  if (!SD.exists("/logs/sensor_readings.csv")) {
    File f = SD.open("/logs/sensor_readings.csv", FILE_WRITE);
    if (f) {
      f.println("timestamp,raw_adc,temp_c,theta,status,qc_valid");
      f.close();
      Serial.println("[INFO] Created sensor_readings.csv");
    }
  }

  if (!SD.exists("/logs/physics_events.csv")) {
    File f = SD.open("/logs/physics_events.csv", FILE_WRITE);
    if (f) {
      f.println("timestamp,event_type,details,cal_state");
      f.close();
      Serial.println("[INFO] Created physics_events.csv");
    }
  }

  // Calculate free space
  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  float freeGB = (float)(totalBytes - usedBytes) / (1024.0 * 1024.0 * 1024.0);

  char msg[50];
  snprintf(msg, sizeof(msg), "SD initialized, %.1f GB free", freeGB);
  logSystem("SD", msg);

  return true;
}

// =============================================================================
// WIFI & CAPTIVE PORTAL
// =============================================================================

void setupWiFi() {
  // Create unique SSID using chip ID
  uint64_t chipId = ESP.getEfuseMac();
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "%s%04X", WIFI_SSID_PREFIX,
           (uint16_t)(chipId & 0xFFFF));

  // Start Access Point
  WiFi.softAP(ssid);
  IPAddress apIP = WiFi.softAPIP();

  Serial.print("[INFO] WiFi AP started: ");
  Serial.println(ssid);
  Serial.print("[INFO] AP IP: ");
  Serial.println(apIP);

  // Start DNS for captive portal
  dnsServer.start(DNS_PORT, "*", apIP);

  // Configure NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  logSystem("WIFI", ssid);
}

// =============================================================================
// WEB SERVER HANDLERS
// =============================================================================

void serveFile(const char *path, const char *contentType) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    webServer.send(404, "text/plain", "File not found");
    return;
  }

  webServer.streamFile(file, contentType);
  file.close();
}

void handleRoot() { serveFile("/index.html", "text/html"); }

void handleApiLatest() {
  StaticJsonDocument<256> doc;
  doc["timestamp"] = lastReadingTimestamp;
  doc["raw"] = lastRaw;
  doc["temp_c"] = lastTemp;
  doc["theta"] = lastTheta;
  doc["status"] = getStatus(lastTheta);

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiData() {
  int hours = 24;
  if (webServer.hasArg("hours")) {
    hours = webServer.arg("hours").toInt();
    if (hours <= 0)
      hours = 24;
  }

  // Stream response to avoid memory issues
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "application/json", "");

  webServer.sendContent("[");

  File file = SD.open("/logs/sensor_readings.csv", FILE_READ);
  if (file) {
    file.readStringUntil('\n'); // Skip header
    bool first = true;

    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.length() > 0) {
        if (!first)
          webServer.sendContent(",");

        // Parse CSV line
        int idx = 0;
        String parts[6];
        int start = 0;
        for (int i = 0; i <= line.length(); i++) {
          if (i == line.length() || line[i] == ',') {
            parts[idx++] = line.substring(start, i);
            start = i + 1;
            if (idx >= 6)
              break;
          }
        }

        // Build JSON object
        String json = "{";
        json += "\"timestamp\":\"" + parts[0] + "\",";
        json += "\"raw\":" + parts[1] + ",";
        json += "\"temp_c\":" + parts[2] + ",";
        json += "\"theta\":" + parts[3] + ",";
        json += "\"status\":\"" + parts[4] + "\",";
        json += "\"qc_valid\":" + parts[5] + "}";

        webServer.sendContent(json);
        first = false;
      }
    }
    file.close();
  }

  webServer.sendContent("]");
}

void handleApiDiagnostics() {
  StaticJsonDocument<512> doc;

  // SD Card status
  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  doc["sd_card"]["status"] = sdCardStatus;
  doc["sd_card"]["free_gb"] =
      (float)(totalBytes - usedBytes) / (1024.0 * 1024.0 * 1024.0);
  doc["sd_card"]["last_write_seconds_ago"] = (millis() - lastSDWrite) / 1000;

  // Sensor status
  doc["sensors"]["soil_status"] = sensorStatus;
  doc["sensors"]["soil_last_raw"] = lastRaw;
  doc["sensors"]["temp_status"] = sensorStatus;
  doc["sensors"]["temp_last_c"] = lastTemp;
  doc["sensors"]["failure_rate_percent"] = 0.2; // Placeholder

  // System status
  doc["system"]["uptime_hours"] = (float)(millis() - bootTime) / 3600000.0;
  doc["system"]["memory_free_kb"] = ESP.getFreeHeap() / 1024;
  doc["system"]["last_reading_seconds_ago"] =
      (millis() - lastReadingTime) / 1000;

  // Calibration (basic - full calibration happens in browser physics.js)
  doc["calibration"]["status"] = "Learning";
  doc["calibration"]["confidence"] = 0.2;
  doc["calibration"]["events_captured"] = 0;

  doc["errors_24h"] = errorCount24h;

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiConfigGet() {
  File file = SD.open("/config/user_prefs.json", FILE_READ);
  if (!file) {
    webServer.send(404, "application/json", "{\"error\":\"Config not found\"}");
    return;
  }

  String json = file.readString();
  file.close();
  webServer.send(200, "application/json", json);
}

void handleApiConfigPost() {
  String body = webServer.arg("plain");

  File file = SD.open("/config/user_prefs.json", FILE_WRITE);
  if (!file) {
    webServer.send(500, "application/json", "{\"error\":\"Write failed\"}");
    return;
  }

  file.print(body);
  file.flush();
  file.close();

  logSystem("CONFIG", "User preferences updated");
  webServer.send(200, "application/json", "{\"success\":true}");
}

void handleApiLogEvent() {
  String body = webServer.arg("plain");

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    webServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  File file = SD.open("/logs/physics_events.csv", FILE_APPEND);
  if (!file) {
    webServer.send(500, "application/json", "{\"error\":\"Write failed\"}");
    return;
  }

  file.print(doc["timestamp"].as<String>());
  file.print(",");
  file.print(doc["event_type"].as<String>());
  file.print(",");
  file.print(doc["details"].as<String>());
  file.print(",");
  file.println(doc["cal_state"].as<String>());
  file.flush();
  file.close();

  lastSDWrite = millis();
  webServer.send(200, "application/json", "{\"success\":true}");
}

void handleNotFound() {
  String path = webServer.uri();

  // Serve static files
  if (path.endsWith(".html")) {
    serveFile(path.c_str(), "text/html");
  } else if (path.endsWith(".css")) {
    serveFile(path.c_str(), "text/css");
  } else if (path.endsWith(".js")) {
    serveFile(path.c_str(), "application/javascript");
  } else if (path.endsWith(".json")) {
    serveFile(path.c_str(), "application/json");
  } else if (path.endsWith(".csv")) {
    serveFile(path.c_str(), "text/csv");
  } else {
    // Captive portal redirect
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
  }
}

void setupWebServer() {
  // Static pages
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/index.html", HTTP_GET, handleRoot);
  webServer.on("/onboarding.html", HTTP_GET,
               []() { serveFile("/onboarding.html", "text/html"); });
  webServer.on("/diagnostics.html", HTTP_GET,
               []() { serveFile("/diagnostics.html", "text/html"); });

  // CSS/JS
  webServer.on("/css/main.css", HTTP_GET,
               []() { serveFile("/css/main.css", "text/css"); });
  webServer.on("/js/physics.js", HTTP_GET,
               []() { serveFile("/js/physics.js", "application/javascript"); });
  webServer.on("/js/app.js", HTTP_GET,
               []() { serveFile("/js/app.js", "application/javascript"); });
  webServer.on("/js/i18n.js", HTTP_GET,
               []() { serveFile("/js/i18n.js", "application/javascript"); });

  // API endpoints
  webServer.on("/api/latest", HTTP_GET, handleApiLatest);
  webServer.on("/api/data", HTTP_GET, handleApiData);
  webServer.on("/api/diagnostics", HTTP_GET, handleApiDiagnostics);
  webServer.on("/api/config", HTTP_GET, handleApiConfigGet);
  webServer.on("/api/config", HTTP_POST, handleApiConfigPost);
  webServer.on("/api/log_event", HTTP_POST, handleApiLogEvent);

  // Catchall
  webServer.onNotFound(handleNotFound);

  webServer.begin();
  Serial.println("[INFO] Web server started on port 80");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("AgriScan Soil Moisture Monitor");
  Serial.print("Firmware v");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("=================================\n");

  bootTime = millis();

  // Initialize LED pins
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setLED(0, 0, 255); // Blue during init

  // Initialize temperature sensor
  tempSensor.begin();
  Serial.println("[INFO] Temperature sensor initialized");

  // Initialize SD card
  if (!initSDCard()) {
    Serial.println(
        "[ERROR] SD card failed - system may not function correctly");
    setLED(255, 0, 255); // Purple for critical error
  }

  // Log boot
  char bootMsg[50];
  snprintf(bootMsg, sizeof(bootMsg), "System started, firmware v%s",
           FIRMWARE_VERSION);
  logSystem("BOOT", bootMsg);

  // Setup WiFi
  setupWiFi();

  // Setup web server
  setupWebServer();

  // Initial sensor reading
  readSensors();
  logSensorReading();
  lastReadingTime = millis();

  // Ready - green blink
  for (int i = 0; i < 3; i++) {
    setLED(0, 255, 0);
    delay(200);
    setLED(0, 0, 0);
    delay(200);
  }

  Serial.println("\n[INFO] AgriScan ready!");
  logSystem("INFO", "System ready");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // Handle DNS (captive portal)
  dnsServer.processNextRequest();

  // Handle web requests
  webServer.handleClient();

  // Read sensors every 15 minutes
  if (millis() - lastReadingTime >= READING_INTERVAL_MS) {
    readSensors();
    logSensorReading();
    lastReadingTime = millis();
  }

  // Update LED pattern
  updateLEDPattern();

  // Reset 24h error count at midnight (simplified)
  static unsigned long lastErrorReset = 0;
  if (millis() - lastErrorReset >= 86400000) { // 24 hours
    errorCount24h = 0;
    lastErrorReset = millis();
  }

  // Small delay to prevent CPU hogging
  delay(10);
}
