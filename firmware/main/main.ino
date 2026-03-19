/**
 * AgriScan Firmware Main
 * Integrates:
 * 1. Sensor Loop (DS18B20 + Capacitive Soil Sensor)
 * 2. Native C++ Physics Engine (no Duktape)
 * 3. SQLite Database
 * 4. Web Server API
 */

#include "db_manager.h"
#include "physics_engine.h"
#include <DHT.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_now.h> // Issue 7: ESP-NOW for CropBand pairing
#include <map>       // Issue 7: per-device physics instances

// =============================================================================
// PIN DEFINITIONS
// =============================================================================

#define SOIL_PIN 34
#define SOIL_SENSOR_2_PIN 35
#define TEMP_PIN 4
// DHT22 humidity sensor — Data→GPIO16, 10kΩ pullup to 3.3V
#define DHT_PIN 16
#define SD_CS 5 // Change if your CS pin is different

// =============================================================================
// GLOBALS
// =============================================================================

AsyncWebServer server(80);
DBManager dbManager("/sd/agriscan.db");

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
DHT dht(DHT_PIN, DHT22);

std::vector<SampleData> sampleBuffer;
const int BATCH_SIZE = 6;

static uint32_t seqTimestamp = 1000000;

// Cache of latest physics output — read by /api/current without DB round-trip
static SensorReading lastReading = {};
static float lastAirTemp = -1.0f;
static float lastHumidity = -1.0f;

// Sensor 2 calibration instance (ADC → VWC conversion only, independent sensor)
static SensorCalibration sensor2Cal;

// Issue 7: ESP-NOW CropBand packet format
typedef struct {
  uint8_t version; // must be 1
  uint16_t raw_adc;
  float temp_c;
  uint32_t timestamp;
  uint8_t crc8;
} CropBandPacket;

// Issue 7: per-device physics instances
std::map<String, PhysicsEngine *> deviceEngines;

// =============================================================================
// CROP CONFIG
// =============================================================================

struct CropConfig {
  bool loaded = false;
  String crop_key = "tomato";
  String soil_key = "loam";
  float theta_fc = 0.31f;
  float theta_wp = 0.14f;
  float theta_refill = 0.21f;
  float Zr_cm = 30.0f;
  float p = 0.5f;
  String stage_name = "early";
  int days_after_planting = 0;
};

CropConfig activeCrop;

// =============================================================================
// THRESHOLD LOADING
// =============================================================================

bool loadThresholds() {
  activeCrop.loaded = false;
  activeCrop.crop_key = "tomato";
  activeCrop.soil_key = "loam";
  activeCrop.days_after_planting = 0;

  File prefsFile = SD.open("/config/user_prefs.json", FILE_READ);
  if (prefsFile) {
    DynamicJsonDocument prefs(2048);
    if (!deserializeJson(prefs, prefsFile)) {
      activeCrop.crop_key = prefs["crop"] | prefs["crop_type"] | "tomato";
      activeCrop.soil_key = prefs["soil"] | "loam";
      long planting_ts = prefs["planting_ts"] | 0L;
      if (planting_ts > 0 && time(nullptr) > planting_ts)
        activeCrop.days_after_planting =
            (int)((time(nullptr) - planting_ts) / 86400);
    }
    prefsFile.close();
  }

  File threshFile = SD.open("/config/crop_thresholds.json", FILE_READ);
  if (!threshFile) {
    Serial.println("[THRESH] crop_thresholds.json not found - using defaults");
    return false;
  }

  DynamicJsonDocument filter(1024);
  filter["crops"][activeCrop.crop_key] = true;
  filter["soils"][activeCrop.soil_key] = true;

  DynamicJsonDocument doc(12288);
  DeserializationError err =
      deserializeJson(doc, threshFile, DeserializationOption::Filter(filter));
  threshFile.close();

  if (err) {
    Serial.printf("[THRESH] Parse error: %s\n", err.c_str());
    return false;
  }

  JsonObject soil = doc["soils"][activeCrop.soil_key];
  if (soil.isNull())
    soil = doc["soils"]["loam"];
  if (soil.isNull()) {
    Serial.println("[THRESH] No soil profile found");
    return false;
  }

  JsonArray stages =
      doc["crops"][activeCrop.crop_key]["stages"].as<JsonArray>();
  if (stages.isNull())
    stages = doc["crops"]["tomato"]["stages"].as<JsonArray>();
  if (stages.isNull() || stages.size() == 0) {
    Serial.println("[THRESH] No stage data found");
    return false;
  }

  activeCrop.theta_fc = soil["theta_fc"] | 0.31f;
  activeCrop.theta_wp = soil["theta_wp"] | 0.14f;

  JsonObject activeStage = stages[stages.size() - 1];
  for (JsonObject stage : stages) {
    int ds = stage["day_start"] | 0;
    int de = stage["day_end"] | 0;
    if (activeCrop.days_after_planting >= ds &&
        activeCrop.days_after_planting <= de) {
      activeStage = stage;
      break;
    }
  }

  activeCrop.p = activeStage["p"] | 0.5f;
  activeCrop.Zr_cm = activeStage["Zr_cm"] | 30.0f;
  activeCrop.stage_name = activeStage["name"] | "early";
  activeCrop.theta_refill =
      activeCrop.theta_fc -
      activeCrop.p * (activeCrop.theta_fc - activeCrop.theta_wp);
  activeCrop.loaded = true;

  Serial.printf("[THRESH] Crop=%s Soil=%s DAP=%d\n",
                activeCrop.crop_key.c_str(), activeCrop.soil_key.c_str(),
                activeCrop.days_after_planting);
  Serial.printf("[THRESH] fc=%.3f wp=%.3f refill=%.3f\n", activeCrop.theta_fc,
                activeCrop.theta_wp, activeCrop.theta_refill);
  return true;
}

// =============================================================================
// ISSUE 11: ADC SENSOR VALIDATION
// =============================================================================

void logSensorError(const char *flag, float value) {
  if (!SD.exists("/logs"))
    SD.mkdir("/logs");
  File f = SD.open("/logs/sensor_errors.csv", FILE_APPEND);
  if (f) {
    f.printf("%lu,%s,%.2f\n", (unsigned long)time(nullptr), flag, value);
    f.close();
  }
}

bool validateSensorReading(int raw_adc, float temp_c) {
  if (raw_adc < 200 || raw_adc > 3900) {
    Serial.printf("[QC] ADC out of range: %d\n", raw_adc);
    logSensorError("ADC_RANGE", (float)raw_adc);
    return false;
  }
  if (temp_c < -10.0f || temp_c > 60.0f) {
    Serial.printf("[QC] Temperature out of range: %.1f\n", temp_c);
    logSensorError("TEMP_RANGE", temp_c);
    return false;
  }
  return true;
}

// =============================================================================
// ISSUE 2: CALIBRATION PERSISTENCE
// =============================================================================

void saveCalibration(const String &deviceMac) {
  if (!SD.exists("/calibration"))
    SD.mkdir("/calibration");
  String path = "/calibration/" + deviceMac + ".json";

  PhysicsEngine *eng = nullptr;
  if (deviceMac == "HUB_ONBOARD") {
    eng = &Physics;
  } else {
    auto it = deviceEngines.find(deviceMac);
    if (it != deviceEngines.end())
      eng = it->second;
  }
  if (!eng)
    return;

  // Serialize calibration state via ArduinoJson
  DynamicJsonDocument doc(1024);
  doc["version"] = 1;
  JsonObject cal = doc.createNestedObject("autoCalibration");
  auto calState = eng->getCalibrationState();
  cal["theta_fc_star"] = calState.theta_fc_star;
  cal["theta_refill_star"] = calState.theta_refill_star;
  cal["n_events"] = calState.n_events;
  cal["n_fc_updates"] = calState.n_fc_updates;
  cal["confidence"] = calState.confidence;

  File f = SD.open(path, FILE_WRITE);
  if (f) {
    serializeJson(doc, f);
    f.close();
    Serial.printf("[CAL] Saved calibration for %s\n", deviceMac.c_str());
  }
}

void loadCalibration(const String &deviceMac) {
  String path = "/calibration/" + deviceMac + ".json";
  if (!SD.exists(path))
    return;

  File f = SD.open(path, FILE_READ);
  if (!f)
    return;

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, f)) {
    f.close();
    return;
  }
  f.close();

  if (doc["version"] != 1)
    return;

  PhysicsEngine *eng = nullptr;
  if (deviceMac == "HUB_ONBOARD") {
    eng = &Physics;
  } else {
    auto it = deviceEngines.find(deviceMac);
    if (it != deviceEngines.end())
      eng = it->second;
  }
  if (!eng)
    return;

  JsonObject cal = doc["autoCalibration"];
  if (!cal.isNull()) {
    eng->restoreCalibrationState(
        {.theta_fc_star = cal["theta_fc_star"] | activeCrop.theta_fc,
         .theta_refill_star =
             cal["theta_refill_star"] | activeCrop.theta_refill,
         .confidence = cal["confidence"] | 0.0f,
         .n_events = cal["n_events"] | 0,
         .n_fc_updates = cal["n_fc_updates"] | 0});
    Serial.printf("[CAL] Restored calibration for %s\n", deviceMac.c_str());
  }
}

// =============================================================================
// ISSUE 7: ESP-NOW CROPBAND PAIRING
// =============================================================================

uint8_t calcCRC8(const uint8_t *data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
  }
  return crc;
}

bool isPairedDevice(const String &mac) {
  File f = SD.open("/config/paired_devices.json", FILE_READ);
  if (!f)
    return false;
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, f)) {
    f.close();
    return false;
  }
  f.close();
  for (JsonObject dev : doc["devices"].as<JsonArray>()) {
    if (dev["mac"] == mac && dev["paired"] == true)
      return true;
  }
  return false;
}

void registerUnknownDevice(const String &mac) {
  // Load existing list
  DynamicJsonDocument doc(4096);
  File fr = SD.open("/config/paired_devices.json", FILE_READ);
  if (fr) {
    deserializeJson(doc, fr);
    fr.close();
  }
  if (!doc.containsKey("devices"))
    doc.createNestedArray("devices");

  // Check not already present
  for (JsonObject dev : doc["devices"].as<JsonArray>()) {
    if (dev["mac"] == mac)
      return; // already registered
  }

  JsonObject entry = doc["devices"].createNestedObject();
  entry["mac"] = mac;
  entry["paired"] = false;

  File fw = SD.open("/config/paired_devices.json", FILE_WRITE);
  if (fw) {
    serializeJson(doc, fw);
    fw.close();
  }
  Serial.printf("[ESPNOW] New device seen: %s\n", mac.c_str());
}

void runPhysicsForDevice(int raw_adc, float temp_c, time_t ts,
                         const String &deviceId) {
  if (deviceEngines.find(deviceId) == deviceEngines.end()) {
    deviceEngines[deviceId] = new PhysicsEngine();
    if (activeCrop.loaded) {
      deviceEngines[deviceId]->configureCropSoil(
          activeCrop.crop_key.c_str(), activeCrop.soil_key.c_str(),
          activeCrop.p, activeCrop.theta_fc, activeCrop.theta_wp,
          activeCrop.theta_refill,
          (long)(time(nullptr) - activeCrop.days_after_planting * 86400L));
    }
  }
  PhysicsEngine *eng = deviceEngines[deviceId];
  SensorReading reading = eng->processSensorReading(raw_adc, temp_c, ts);
  Serial.printf("[ESPNOW] Device %s theta=%.3f status=%s\n", deviceId.c_str(),
                reading.theta, reading.status);
  saveCalibration(deviceId);
}

void onEspNowReceive(const esp_now_recv_info *recv_info, const uint8_t *data, int len) {
  if (len < (int)sizeof(CropBandPacket))
    return;

  CropBandPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  // Validate CRC (over all bytes except last)
  uint8_t expected = calcCRC8(data, sizeof(pkt) - 1);
  if (pkt.crc8 != expected) {
    Serial.println("[ESPNOW] CRC mismatch — packet dropped");
    return;
  }
  if (pkt.version != 1)
    return;

  // Build MAC string
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  String macString(macStr);

  if (!isPairedDevice(macString)) {
    registerUnknownDevice(macString);
    return; // ignore data from unpaired devices
  }

  time_t ts = (time_t)pkt.timestamp;
  if (ts < 1000000)
    ts = time(nullptr);

  if (validateSensorReading(pkt.raw_adc, pkt.temp_c)) {
    runPhysicsForDevice(pkt.raw_adc, pkt.temp_c, ts, macString);
  }
}

void initEspNow() {
  esp_now_init();
  esp_now_register_recv_cb(onEspNowReceive);
  Serial.println("[ESPNOW] Receiver initialized");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Sensor checks
  int soilRaw = analogRead(SOIL_PIN);
  Serial.println(soilRaw > 0 && soilRaw < 4095
                     ? "✅ Soil sensor 1 (GPIO34) CONNECTED — Raw: " + String(soilRaw)
                     : "❌ Soil sensor 1 (GPIO34) NOT connected");

  int soilRaw2 = analogRead(SOIL_SENSOR_2_PIN);
  Serial.println(soilRaw2 > 0 && soilRaw2 < 4095
                     ? "✅ Soil sensor 2 (GPIO35) CONNECTED — Raw: " + String(soilRaw2)
                     : "❌ Soil sensor 2 (GPIO35) NOT connected");

  tempSensor.begin();
  if (tempSensor.getDeviceCount() > 0) {
    tempSensor.requestTemperatures();
    Serial.println("✅ DS18B20 CONNECTED — Temp: " +
                   String(tempSensor.getTempCByIndex(0)) + "°C");
  } else {
    Serial.println("❌ DS18B20 NOT found — check wiring + 4.7kΩ pullup");
  }

  dht.begin();
  delay(2000); // DHT22 needs ~2 s after power-up before first read
  float dhtH = dht.readHumidity();
  float dhtT = dht.readTemperature();
  Serial.println(!isnan(dhtH)
                     ? "✅ DHT22 CONNECTED — Humidity: " + String(dhtH, 1) + "% Temp: " + String(dhtT, 1) + "°C"
                     : "❌ DHT22 NOT found — check GPIO16, 10kΩ pullup to 3.3V");

  // SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card FAILED — check wiring and CS pin");
    return;
  }
  Serial.println("✅ SD card OK");
  Serial.println(SD.exists("/www/index.html") ? "✅ index.html FOUND"
                                              : "❌ index.html MISSING");

  // Config
  if (!SD.exists("/config"))
    SD.mkdir("/config");
  if (!SD.exists("/config/user_prefs.json")) {
    File f = SD.open("/config/user_prefs.json", FILE_WRITE);
    if (f) {
      f.print("{\"onboarding_complete\":false,\"device_name\":\"\","
              "\"root_depth_cm\":30,\"crop\":\"tomato\",\"soil\":\"loam\","
              "\"setup_date\":null,\"planting_ts\":null,"
              "\"farmer_name\":\"\",\"notes\":\"\"}");
      f.close();
      Serial.println("[BOOT] Created default user_prefs.json");
    }
  }

  // Database
  if (!dbManager.init())
    Serial.println("[BOOT] DB Init Failed");

  // Physics engine - native C++, no Duktape
  if (loadThresholds() && activeCrop.loaded) {
    Physics.configureCropSoil(
        activeCrop.crop_key.c_str(), activeCrop.soil_key.c_str(), activeCrop.p,
        activeCrop.theta_fc, activeCrop.theta_wp, activeCrop.theta_refill,
        (long)(time(nullptr) - activeCrop.days_after_planting * 86400L));
  } else {
    Serial.println("[BOOT] Using physics defaults");
  }

  // Issue 2: restore saved calibration state
  loadCalibration("HUB_ONBOARD");

  // WiFi AP
  WiFi.softAP("AgriScan_Connect", "agri1234");
  // Issue 7: initialize ESP-NOW after WiFi
  initEspNow();
  Serial.println("[BOOT] WiFi: AgriScan_Connect | http://192.168.4.1");

  // Web server
  server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

  server.on("/api/current", HTTP_GET, [](AsyncWebServerRequest *req) {
    SampleData s = dbManager.getLatestSample();
    String json = "{";
    json += "\"timestamp\":" + String(s.timestamp) + ",";
    json += "\"raw_adc\":" + String(s.raw_adc) + ",";
    json += "\"raw_adc_2\":" + String(s.raw_adc_2) + ",";
    json += "\"temp_c\":" + String(s.temp_c, 1) + ",";
    json += "\"soil_temp_c\":" + String(s.temp_c, 1) + ",";
    json += "\"air_temp_c\":" + String(lastAirTemp, 1) + ",";
    json += "\"humidity\":" + String(lastHumidity, 1) + ",";
    json += "\"theta\":" + String(s.theta, 4) + ",";
    json += "\"theta_2\":" + String(s.theta_2, 4) + ",";
    json += "\"psi_kpa\":" + String(s.psi_kpa, 2) + ",";
    json += "\"aw_mm\":" + String(s.aw_mm, 1) + ",";
    json += "\"Se\":" + String(lastReading.Se, 4) + ",";
    json += "\"TAW_mm\":" + String(lastReading.TAW_mm, 1) + ",";
    json += "\"Dr_mm\":" + String(lastReading.Dr_mm, 1) + ",";
    json += "\"fractionDepleted\":" + String(lastReading.fractionDepleted, 3) + ",";
    json += "\"dryingRate_per_hr\":" + String(lastReading.dryingRate_per_hr, 4) + ",";
    json += "\"regime\":\"" + String(lastReading.regime) + "\",";
    json += "\"status\":\"" + s.status + "\",";
    json += "\"urgency\":\"" + s.urgency + "\",";
    json += "\"recommendation\":\"" + String(lastReading.recommendation) + "\",";
    json += "\"calibration_state\":\"" + String(lastReading.calibration_state) + "\",";
    json += "\"qc_valid\":" + String(lastReading.qc_valid ? "true" : "false") + ",";
    json += "\"confidence\":" + String(s.confidence, 2) + ",";
    json += "\"theta_fc\":" + String(activeCrop.theta_fc, 3) + ",";
    json += "\"theta_refill\":" + String(activeCrop.theta_refill, 3) + ",";
    json += "\"stage\":\"" + activeCrop.stage_name + "\",";
    json += "\"crop\":\"" + activeCrop.crop_key + "\"";
    json += "}";
    req->send(200, "application/json", json);
  });

  server.on("/api/series", HTTP_GET, [](AsyncWebServerRequest *req) {
    int limit = 144;
    if (req->hasParam("limit"))
      limit = req->getParam("limit")->value().toInt();
    if (limit < 1 || limit > 200)
      limit = 144;

    auto series = dbManager.getRecentSamples(limit);
    String json = "[";
    for (size_t i = 0; i < series.size(); i++) {
      if (i > 0)
        json += ",";
      json += "{\"timestamp\":" + String(series[i].timestamp) + ",";
      json += "\"theta\":" + String(series[i].theta, 4) + ",";
      json += "\"theta_2\":" + String(series[i].theta_2, 4) + ",";
      json += "\"raw_adc_2\":" + String(series[i].raw_adc_2) + ",";
      json += "\"humidity\":" + String(series[i].humidity, 1) + "}";
    }
    json += "]";
    req->send(200, "application/json", json);
  });

  server.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest *req) {
    DynamicJsonDocument doc(4096);
    File f = SD.open("/config/paired_devices.json", FILE_READ);
    if (f) {
      deserializeJson(doc, f);
      f.close();
    }
    if (!doc.containsKey("devices"))
      doc.createNestedArray("devices");

    String json = "[";
    bool first = true;
    for (JsonObject dev : doc["devices"].as<JsonArray>()) {
      if (!first)
        json += ",";
      first = false;
      String mac = dev["mac"] | "";
      bool paired = dev["paired"] | false;
      bool online = (deviceEngines.find(mac) != deviceEngines.end());
      json += "{\"mac\":\"" + mac + "\",";
      json += "\"paired\":" + String(paired ? "true" : "false") + ",";
      json += "\"online\":" + String(online ? "true" : "false") + "}";
    }
    json += "]";
    req->send(200, "application/json", json);
  });

  server.on("/api/devices/approve", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!req->hasParam("mac")) {
      req->send(400, "application/json", "{\"error\":\"mac param required\"}");
      return;
    }
    String mac = req->getParam("mac")->value();

    DynamicJsonDocument doc(4096);
    File fr = SD.open("/config/paired_devices.json", FILE_READ);
    if (fr) {
      deserializeJson(doc, fr);
      fr.close();
    }
    if (!doc.containsKey("devices"))
      doc.createNestedArray("devices");

    bool found = false;
    for (JsonObject dev : doc["devices"].as<JsonArray>()) {
      if (dev["mac"] == mac) {
        dev["paired"] = true;
        found = true;
        break;
      }
    }
    if (!found) {
      req->send(404, "application/json", "{\"error\":\"device not found\"}");
      return;
    }
    File fw = SD.open("/config/paired_devices.json", FILE_WRITE);
    if (!fw) {
      req->send(500, "application/json", "{\"error\":\"write failed\"}");
      return;
    }
    serializeJson(doc, fw);
    fw.close();
    Serial.printf("[PAIR] Approved %s\n", mac.c_str());
    req->send(200, "application/json", "{\"success\":true}");
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    File f = SD.open("/config/user_prefs.json", FILE_READ);
    if (!f) {
      req->send(200, "application/json", "{\"onboarding_complete\":false}");
      return;
    }
    String content = f.readString();
    f.close();
    req->send(200, "application/json", content);
  });

  server.on(
      "/api/config", HTTP_POST,
      [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", "{\"success\":true}");
      },
      nullptr,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index,
         size_t total) {
        static String configBody;
        if (index == 0)
          configBody = "";
        for (size_t i = 0; i < len; i++)
          configBody += (char)data[i];
        if (index + len >= total) {
          File f = SD.open("/config/user_prefs.json", FILE_WRITE);
          if (f) {
            f.print(configBody);
            f.close();
          }
          loadThresholds();
        }
      });

  server.on("/api/diagnostics", HTTP_GET, [](AsyncWebServerRequest *req) {
    String json = "{";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime_s\":" + String(millis() / 1000) + ",";
    json += "\"sample_count\":" + String(seqTimestamp - 1000000) + ",";
    json += "\"crop\":\"" + activeCrop.crop_key + "\",";
    json += "\"soil\":\"" + activeCrop.soil_key + "\",";
    json += "\"paired_devices\":" + String(deviceEngines.size());
    json += "}";
    req->send(200, "application/json", json);
  });

  // Issue 13: SD card storage management endpoints
  server.on("/api/storage", HTTP_GET, [](AsyncWebServerRequest *req) {
    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    uint64_t free_ = total - used;
    String json = "{";
    json += "\"total_mb\":" + String((float)total / 1048576.0f, 1) + ",";
    json += "\"used_mb\":" + String((float)used / 1048576.0f, 1) + ",";
    json += "\"free_mb\":" + String((float)free_ / 1048576.0f, 1);
    json += "}";
    req->send(200, "application/json", json);
  });

  server.on("/api/logs/download", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!SD.exists("/logs/readings.csv")) {
      req->send(404, "text/plain", "Log file not found");
      return;
    }
    req->send(SD, "/logs/readings.csv", "text/csv", true);
  });

  server.on("/api/logs/clear", HTTP_DELETE, [](AsyncWebServerRequest *req) {
    if (SD.exists("/logs/readings.csv"))
      SD.remove("/logs/readings.csv");
    // Recreate with header row
    File f = SD.open("/logs/readings.csv", FILE_WRITE);
    if (f) {
      f.println("timestamp,raw_adc,temp_c,humidity,theta,status,urgency");
      f.close();
    }
    req->send(200, "application/json", "{\"success\":true}");
  });

  server.onNotFound(
      [](AsyncWebServerRequest *req) { req->redirect("http://192.168.4.1"); });

  server.begin();
  Serial.println("[BOOT] AgriScan ready");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  static unsigned long lastSample = 0;

  if (millis() - lastSample > 2000) {
    lastSample = millis();

    int raw = analogRead(SOIL_PIN);
    int raw2 = analogRead(SOIL_SENSOR_2_PIN);
    tempSensor.requestTemperatures();
    float temp = tempSensor.getTempCByIndex(0);
    if (temp == DEVICE_DISCONNECTED_C)
      temp = 25.0f;
    float humidity = dht.readHumidity();
    if (isnan(humidity))
      humidity = -1.0f;
    float air_temp = dht.readTemperature();
    if (isnan(air_temp))
      air_temp = -1.0f;

    time_t ts = time(nullptr);
    if (ts < 1000000)
      ts = seqTimestamp++;

    // DHT22 is independent — update cache regardless of soil/temp validation
    if (humidity >= 0) {
      lastHumidity = humidity;
      lastAirTemp = air_temp;
    }

    // Issue 11: validate before processing
    if (!validateSensorReading(raw, temp)) {
      Serial.println("[QC] Reading skipped");
    } else {
      // Native C++ physics - no JS, no Duktape
      SensorReading reading = Physics.processSensorReading(raw, temp, ts);
      lastReading = reading;

      // Sensor 2: apply same calibration curve independently
      float theta2 = sensor2Cal.calibrate(raw2, temp);

      SampleData s;
      s.timestamp = reading.timestamp;
      s.raw_adc = reading.raw_adc;
      s.raw_adc_2 = raw2;
      s.temp_c = reading.temp_c;
      s.humidity = humidity;
      s.air_temp_c = air_temp;
      s.theta = reading.theta;
      s.theta_2 = theta2;
      s.theta_fc = reading.theta_fc;
      s.theta_refill = reading.theta_refill;
      s.psi_kpa = reading.psi_kPa;
      s.aw_mm = reading.AW_mm;
      s.fraction_depleted = reading.fractionDepleted;
      s.drying_rate = reading.dryingRate_per_hr;
      s.regime = String(reading.regime);
      s.status = String(reading.status);
      s.urgency = String(reading.urgency);
      s.confidence = reading.confidence;
      s.qc_valid = reading.qc_valid;
      s.seq = (int)(seqTimestamp - 1000000);

      Serial.printf("[SENSOR1] raw=%d theta=%.3f status=%s urgency=%s conf=%.2f\n",
                    raw, reading.theta, reading.status, reading.urgency,
                    reading.confidence);
      Serial.printf("[SENSOR2] raw=%d theta=%.3f\n", raw2, theta2);

      sampleBuffer.push_back(s);
      if ((int)sampleBuffer.size() >= BATCH_SIZE) {
        dbManager.writeSampleBatch(sampleBuffer);
        sampleBuffer.clear();
        Serial.println("[DB] Batch flushed");
      }

      // Issue 2: persist calibration state after each reading
      saveCalibration("HUB_ONBOARD");
    }
  }
}
