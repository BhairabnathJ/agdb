/**
 * AgriScan Firmware Main
 * Integrates:
 * 1. Sensor Loop
 * 2. Duktape JS Engine (Physics)
 * 3. SQLite Database
 * 4. Web Server API
 */

#include "db_manager.h"
#include "duktape.h" // Requires Duktape library
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPIFFS.h>
#include <WiFi.h>

// Globals
AsyncWebServer server(80);
DBManager dbManager("/sd/agriscan.db");
std::vector<SampleData> sampleBuffer;
const int BATCH_SIZE = 6;

// JS Context
duk_context *ctx = nullptr;

// --- DUKTAPE HELPERS ---
void setupJS() {
  ctx = duk_create_heap_default();

  // Load physics.js from SD
  File f = SD.open("/physics.js");
  if (!f) {
    Serial.println("Failed to load physics.js");
    return;
  }
  String code = f.readString();
  f.close();

  if (duk_peval_string(ctx, code.c_str()) != 0) {
    Serial.printf("JS Load Error: %s\n", duk_safe_to_string(ctx, -1));
  } else {
    Serial.println("Physics Engine Loaded");
  }
  duk_pop(ctx); // Pop result
}

SampleData runPhysics(int raw, float temp, time_t ts) {
  SampleData s = {};
  s.timestamp = ts;
  s.raw_adc = raw;
  s.temp_c = temp;

  // Call JS: Physics.processSensorReading(raw, temp, ts)
  char call[128];
  snprintf(call, sizeof(call), "Physics.processSensorReading(%d, %.2f, %ld)",
           raw, temp, ts);

  if (duk_peval_string(ctx, call) != 0) {
    Serial.printf("JS Exec Error: %s\n", duk_safe_to_string(ctx, -1));
  } else {
    // Extract Object
    duk_get_prop_string(ctx, -1, "theta");
    s.theta = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "status");
    s.status = String(duk_get_string(ctx, -1));
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "psi_kpa");
    s.psi_kpa = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "aw_mm");
    s.aw_mm = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "confidence");
    s.confidence = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    // ... extract other fields as needed
  }
  duk_pop(ctx); // Pop result object
  return s;
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);

  // Init SD
  if (!SD.begin()) {
    Serial.println("SD Init Failed");
    return;
  }

  // Init DB
  if (!dbManager.init()) {
    Serial.println("DB Init Failed");
  }

  // Init JS
  setupJS();

  // Init WiFi AP
  WiFi.softAP("AgriScan_Connect", "agri1234");

  // --- API ENDPOINTS (From sch.txt) ---

  // 1. Serve Static Files
  server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

  // 2. GET /api/current
  server.on("/api/current", HTTP_GET, [](AsyncWebServerRequest *request) {
    SampleData latest = dbManager.getLatestSample();
    String json = "{";
    json += "\"timestamp\":" + String(latest.timestamp) + ",";
    json += "\"theta\":" + String(latest.theta, 4) + ",";
    json += "\"psi_kpa\":" + String(latest.psi_kpa, 2) + ",";
    json += "\"status\":\"" + latest.status + "\",";
    json += "\"urgency\":\"" + latest.urgency + "\",";
    json += "\"confidence\":" + String(latest.confidence, 2);
    json += "}";
    request->send(200, "application/json", json);
  });

  // 3. GET /api/series
  server.on("/api/series", HTTP_GET, [](AsyncWebServerRequest *request) {
    long start = 0;
    long end = 0;
    if (request->hasParam("start"))
      start = request->getParam("start")->value().toInt();
    if (request->hasParam("end"))
      end = request->getParam("end")->value().toInt();

    std::vector<SampleData> series = dbManager.getSamplesInRange(start, end);
    String json = "[";
    for (size_t i = 0; i < series.size(); i++) {
      if (i > 0)
        json += ",";
      json += "{\"timestamp\":" + String(series[i].timestamp) + ",";
      json += "\"theta\":" + String(series[i].theta, 4) + "}";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.begin();
}

// --- LOOP ---
void loop() {
  static unsigned long lastSample = 0;
  if (millis() - lastSample > 10000) { // Every 10s (Simulated fast loop)
    lastSample = millis();

    // 1. Read Sensors
    int raw = analogRead(34);
    float temp = 25.0; // Stub

    // 2. Process Physics (JS)
    SampleData s = runPhysics(raw, temp, time(nullptr));

    // 3. Batch Buffer
    sampleBuffer.push_back(s);

    // 4. Flush to DB if Batch Full
    if (sampleBuffer.size() >= BATCH_SIZE) {
      dbManager.writeSampleBatch(sampleBuffer);
      sampleBuffer.clear();
      Serial.println("Batch flushed to SQLite");
    }
  }
}
