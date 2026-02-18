/**
 * AgriScan Firmware Main
 * Integrates:
 * 1. Sensor Loop
 * 2. Duktape JS Engine (Physics)
 * 3. SQLite Database
 * 4. Web Server API
 */

#include "db_manager.h"
#include "duktape.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPIFFS.h>
#include <WiFi.h>

// =============================================================================
// GLOBALS
// =============================================================================

AsyncWebServer server(80);
DBManager dbManager("/sd/agriscan.db");
std::vector<SampleData> sampleBuffer;
const int BATCH_SIZE = 6;

duk_context *ctx = nullptr;

// =============================================================================
// CROP CONFIG STRUCT
// =============================================================================

struct StageConfig {
  String name;
  int day_start;
  int day_end;
  float Zr_cm;
  float p;
};

struct CropConfig {
  bool loaded = false;
  String crop_key; // "maize", "wheat", "tomato", "potato"
  String soil_key; // "sandy_loam", "loam", "clay_loam", "clay"
  float theta_fc;
  float theta_wp;
  float theta_refill; // Computed: theta_fc - p*(theta_fc - theta_wp)
  float Zr_cm;
  float p;
  String stage_name;
  int days_after_planting;
};

CropConfig activeCrop;

// =============================================================================
// THRESHOLD LOADING
// =============================================================================

/**
 * Reads user_prefs.json to get crop, soil, and planting_date.
 * Reads crop_thresholds.json to get stage params.
 * Computes theta_refill.
 * Stores result in activeCrop global.
 */
bool loadThresholds() {
  activeCrop.loaded = false;
  activeCrop.crop_key = "tomato";
  activeCrop.soil_key = "loam";
  activeCrop.days_after_planting = 0;

  File prefsFile = SD.open("/config/user_prefs.json", FILE_READ);
  if (prefsFile) {
    DynamicJsonDocument prefs(2048);
    DeserializationError err = deserializeJson(prefs, prefsFile);
    prefsFile.close();
    if (!err) {
      activeCrop.crop_key = prefs["crop"] | prefs["crop_type"] | "tomato";
      activeCrop.soil_key = prefs["soil"] | "loam";
      long planting_ts = prefs["planting_ts"] | 0L;
      if (planting_ts > 0 && time(nullptr) > planting_ts) {
        activeCrop.days_after_planting = (int)((time(nullptr) - planting_ts) / 86400);
      }
    }
  }

  File threshFile = SD.open("/config/crop_thresholds.json", FILE_READ);
  if (!threshFile) {
    Serial.println("[THRESH] crop_thresholds.json not found");
    return false;
  }

  DynamicJsonDocument filter(1024);
  filter["crops"][activeCrop.crop_key] = true;
  filter["soils"][activeCrop.soil_key] = true;
  filter["crops"]["tomato"] = true;
  filter["soils"]["loam"] = true;

  DynamicJsonDocument doc(12288);
  DeserializationError err =
      deserializeJson(doc, threshFile, DeserializationOption::Filter(filter));
  threshFile.close();
  if (err) {
    Serial.printf("[THRESH] crop_thresholds parse error: %s\n", err.c_str());
    return false;
  }

  JsonObject soil = doc["soils"][activeCrop.soil_key];
  if (soil.isNull()) {
    activeCrop.soil_key = "loam";
    soil = doc["soils"]["loam"];
  }
  if (soil.isNull()) {
    Serial.println("[THRESH] No usable soil profile found");
    return false;
  }

  JsonArray stages = doc["crops"][activeCrop.crop_key]["stages"].as<JsonArray>();
  if (stages.isNull()) {
    activeCrop.crop_key = "tomato";
    stages = doc["crops"]["tomato"]["stages"].as<JsonArray>();
  }
  if (stages.isNull() || stages.size() == 0) {
    Serial.println("[THRESH] No usable crop stage data found");
    return false;
  }

  activeCrop.theta_fc = soil["theta_fc"] | 0.31f;
  activeCrop.theta_wp = soil["theta_wp"] | 0.14f;

  JsonObject activeStage = stages[stages.size() - 1];
  for (JsonObject stage : stages) {
    int ds = stage["day_start"] | 0;
    int de = stage["day_end"] | 0;
    if (activeCrop.days_after_planting >= ds && activeCrop.days_after_planting <= de) {
      activeStage = stage;
      break;
    }
  }

  activeCrop.p = activeStage["p"] | 0.5f;
  activeCrop.Zr_cm = activeStage["Zr_cm"] | 30.0f;
  activeCrop.stage_name = activeStage["name"] | "early";
  activeCrop.theta_refill =
      activeCrop.theta_fc - activeCrop.p * (activeCrop.theta_fc - activeCrop.theta_wp);
  activeCrop.loaded = true;

  Serial.printf("[THRESH] Crop: %s | Soil: %s | DAP: %d\n",
                activeCrop.crop_key.c_str(), activeCrop.soil_key.c_str(),
                activeCrop.days_after_planting);
  Serial.printf("[THRESH] Stage: %s | Zr: %.0fcm | p: %.2f\n",
                activeCrop.stage_name.c_str(), activeCrop.Zr_cm, activeCrop.p);
  Serial.printf("[THRESH] theta_fc=%.3f | theta_wp=%.3f | theta_refill=%.3f\n",
                activeCrop.theta_fc, activeCrop.theta_wp, activeCrop.theta_refill);
  return true;
}

// =============================================================================
// DUKTAPE HELPERS
// =============================================================================

void setupJS() {
  ctx = duk_create_heap_default();

  File f = SD.open("/js/physics.js");
  if (!f) {
    Serial.println("[JS] Failed to load physics.js");
    return;
  }
  String code = f.readString();
  f.close();

  if (duk_peval_string(ctx, code.c_str()) != 0) {
    Serial.printf("[JS] Load Error: %s\n", duk_safe_to_string(ctx, -1));
  } else {
    Serial.println("[JS] Physics Engine Loaded");
  }
  duk_pop(ctx);
}

/**
 * Push crop/soil config into the running Physics JS instance.
 * Called once after setupJS() and loadThresholds().
 * Re-call this if user changes crop or soil type mid-session.
 */
void initPhysicsWithCrop() {
  if (!activeCrop.loaded) {
    Serial.println("[JS] Skipping crop init - config not loaded");
    return;
  }

  char call[384];

  snprintf(call, sizeof(call),
           "if (typeof Physics !== 'undefined' && Physics.configureCropSoil) {"
           "Physics.configureCropSoil({crop:'%s',soil:'%s',p:%.4f,theta_fc:%.4f,"
           "theta_wp:%.4f,theta_refill:%.4f,planting_ts:%ld});}",
           activeCrop.crop_key.c_str(), activeCrop.soil_key.c_str(), activeCrop.p,
           activeCrop.theta_fc, activeCrop.theta_wp, activeCrop.theta_refill,
           (long)(time(nullptr) - (activeCrop.days_after_planting * 86400)));

  if (duk_peval_string(ctx, call) != 0) {
    Serial.printf("[JS] Crop init error: %s\n", duk_safe_to_string(ctx, -1));
  } else {
    Serial.printf("[JS] Physics configured for %s / %s stage\n",
                  activeCrop.crop_key.c_str(), activeCrop.stage_name.c_str());
  }
  duk_pop(ctx);
}

// =============================================================================
// PHYSICS CALL
// =============================================================================

SampleData runPhysics(int raw, float temp, time_t ts) {
  SampleData s = {};
  s.timestamp = ts;
  s.raw_adc = raw;
  s.temp_c = temp;

  char call[128];
  snprintf(call, sizeof(call), "Physics.processSensorReading(%d, %.2f, %ld)",
           raw, temp, ts);

  if (duk_peval_string(ctx, call) != 0) {
    Serial.printf("[JS] Exec Error: %s\n", duk_safe_to_string(ctx, -1));
  } else {
    duk_get_prop_string(ctx, -1, "theta");
    s.theta = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "status");
    s.status = String(duk_get_string(ctx, -1));
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "psi_kPa");
    s.psi_kpa = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "AW_mm");
    s.aw_mm = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "confidence");
    s.confidence = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "urgency");
    s.urgency = String(duk_get_string(ctx, -1));
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "fractionDepleted");
    s.fraction_depleted = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "dryingRate_per_hr");
    s.drying_rate = (float)duk_get_number(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "regime");
    s.regime = String(duk_get_string(ctx, -1));
    duk_pop(ctx);
  }
  duk_pop(ctx);
  return s;
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);

  if (!SD.begin()) {
    Serial.println("[BOOT] SD Init Failed");
    return;
  }

  if (!SD.exists("/config")) {
    SD.mkdir("/config");
  }
  if (!SD.exists("/config/user_prefs.json")) {
    File defaults = SD.open("/config/user_prefs.json", FILE_WRITE);
    if (defaults) {
      defaults.print(
          "{\"onboarding_complete\":false,\"device_name\":\"\","
          "\"root_depth_cm\":30,\"crop\":\"tomato\",\"soil\":\"loam\","
          "\"setup_date\":null,\"planting_ts\":null,\"farmer_name\":\"\","
          "\"notes\":\"\"}");
      defaults.close();
      Serial.println("[BOOT] Created default /config/user_prefs.json");
    }
  }

  if (!dbManager.init()) {
    Serial.println("[BOOT] DB Init Failed");
  }

  // Load JS physics engine
  setupJS();

  // Load crop/soil thresholds and configure physics
  if (loadThresholds()) {
    initPhysicsWithCrop();
  } else {
    Serial.println("[BOOT] Using physics.js defaults (no threshold file)");
  }

  WiFi.softAP("AgriScan_Connect", "agri1234");

  // --- API ENDPOINTS ---

  server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

  server.on("/api/current", HTTP_GET, [](AsyncWebServerRequest *request) {
    SampleData latest = dbManager.getLatestSample();
    String json = "{";
    json += "\"timestamp\":" + String(latest.timestamp) + ",";
    json += "\"theta\":" + String(latest.theta, 4) + ",";
    json += "\"psi_kpa\":" + String(latest.psi_kpa, 2) + ",";
    json += "\"status\":\"" + latest.status + "\",";
    json += "\"urgency\":\"" + latest.urgency + "\",";
    json += "\"confidence\":" + String(latest.confidence, 2) + ",";
    // Expose threshold context to dashboard
    json += "\"theta_fc\":" + String(activeCrop.theta_fc, 3) + ",";
    json += "\"theta_refill\":" + String(activeCrop.theta_refill, 3) + ",";
    json += "\"stage\":\"" + activeCrop.stage_name + "\",";
    json += "\"crop\":\"" + activeCrop.crop_key + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/series", HTTP_GET, [](AsyncWebServerRequest *request) {
    long start = 0, end = 0;
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
  Serial.println("[BOOT] AgriScan ready");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  static unsigned long lastSample = 0;
  if (millis() - lastSample > 10000) {
    lastSample = millis();

    int raw = analogRead(34);
    float temp = 25.0; // Replace with DS18B20 read

    SampleData s = runPhysics(raw, temp, time(nullptr));

    sampleBuffer.push_back(s);

    if (sampleBuffer.size() >= BATCH_SIZE) {
      dbManager.writeSampleBatch(sampleBuffer);
      sampleBuffer.clear();
      Serial.println("[DB] Batch flushed");
    }
  }
}
