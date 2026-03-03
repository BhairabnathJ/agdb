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
#include <Arduino.h>
#include <ArduinoJson.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <SD.h>
#include <WiFi.h>

// =============================================================================
// PIN DEFINITIONS
// =============================================================================

#define SOIL_PIN  34
#define TEMP_PIN  4
#define SD_CS     5    // Change if your CS pin is different

// =============================================================================
// GLOBALS
// =============================================================================

AsyncWebServer server(80);
DBManager      dbManager("/sd/agriscan.db");

OneWire           oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

std::vector<SampleData> sampleBuffer;
const int BATCH_SIZE = 6;

static uint32_t seqTimestamp = 1000000;

// =============================================================================
// CROP CONFIG
// =============================================================================

struct CropConfig {
    bool    loaded              = false;
    String  crop_key            = "tomato";
    String  soil_key            = "loam";
    float   theta_fc            = 0.31f;
    float   theta_wp            = 0.14f;
    float   theta_refill        = 0.21f;
    float   Zr_cm               = 30.0f;
    float   p                   = 0.5f;
    String  stage_name          = "early";
    int     days_after_planting = 0;
};

CropConfig activeCrop;

// =============================================================================
// THRESHOLD LOADING
// =============================================================================

bool loadThresholds() {
    activeCrop.loaded              = false;
    activeCrop.crop_key            = "tomato";
    activeCrop.soil_key            = "loam";
    activeCrop.days_after_planting = 0;

    File prefsFile = SD.open("/config/user_prefs.json", FILE_READ);
    if (prefsFile) {
        DynamicJsonDocument prefs(2048);
        if (!deserializeJson(prefs, prefsFile)) {
            activeCrop.crop_key = prefs["crop"] | prefs["crop_type"] | "tomato";
            activeCrop.soil_key = prefs["soil"] | "loam";
            long planting_ts    = prefs["planting_ts"] | 0L;
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
    if (soil.isNull()) soil = doc["soils"]["loam"];
    if (soil.isNull()) {
        Serial.println("[THRESH] No soil profile found");
        return false;
    }

    JsonArray stages = doc["crops"][activeCrop.crop_key]["stages"].as<JsonArray>();
    if (stages.isNull()) stages = doc["crops"]["tomato"]["stages"].as<JsonArray>();
    if (stages.isNull() || stages.size() == 0) {
        Serial.println("[THRESH] No stage data found");
        return false;
    }

    activeCrop.theta_fc = soil["theta_fc"] | 0.31f;
    activeCrop.theta_wp = soil["theta_wp"] | 0.14f;

    JsonObject activeStage = stages[stages.size() - 1];
    for (JsonObject stage : stages) {
        int ds = stage["day_start"] | 0;
        int de = stage["day_end"]   | 0;
        if (activeCrop.days_after_planting >= ds &&
            activeCrop.days_after_planting <= de) {
            activeStage = stage;
            break;
        }
    }

    activeCrop.p          = activeStage["p"]     | 0.5f;
    activeCrop.Zr_cm      = activeStage["Zr_cm"] | 30.0f;
    activeCrop.stage_name = activeStage["name"]  | "early";
    activeCrop.theta_refill =
        activeCrop.theta_fc - activeCrop.p *
        (activeCrop.theta_fc - activeCrop.theta_wp);
    activeCrop.loaded = true;

    Serial.printf("[THRESH] Crop=%s Soil=%s DAP=%d\n",
                  activeCrop.crop_key.c_str(),
                  activeCrop.soil_key.c_str(),
                  activeCrop.days_after_planting);
    Serial.printf("[THRESH] fc=%.3f wp=%.3f refill=%.3f\n",
                  activeCrop.theta_fc, activeCrop.theta_wp, activeCrop.theta_refill);
    return true;
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
        ? "✅ Soil sensor CONNECTED — Raw: " + String(soilRaw)
        : "❌ Soil sensor NOT connected");

    tempSensor.begin();
    if (tempSensor.getDeviceCount() > 0) {
        tempSensor.requestTemperatures();
        Serial.println("✅ DS18B20 CONNECTED — Temp: " +
                       String(tempSensor.getTempCByIndex(0)) + "°C");
    } else {
        Serial.println("❌ DS18B20 NOT found — check wiring + 4.7kΩ pullup");
    }

    // SD card
    if (!SD.begin(SD_CS)) {
        Serial.println("❌ SD card FAILED — check wiring and CS pin");
        return;
    }
    Serial.println("✅ SD card OK");
    Serial.println(SD.exists("/www/index.html")
        ? "✅ index.html FOUND" : "❌ index.html MISSING");

    // Config
    if (!SD.exists("/config")) SD.mkdir("/config");
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
            activeCrop.crop_key.c_str(),
            activeCrop.soil_key.c_str(),
            activeCrop.p,
            activeCrop.theta_fc,
            activeCrop.theta_wp,
            activeCrop.theta_refill,
            (long)(time(nullptr) - activeCrop.days_after_planting * 86400L)
        );
    } else {
        Serial.println("[BOOT] Using physics defaults");
    }

    // WiFi AP
    WiFi.softAP("AgriScan_Connect", "agri1234");
    Serial.println("[BOOT] WiFi: AgriScan_Connect | http://192.168.4.1");

    // Web server
    server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

    server.on("/api/current", HTTP_GET, [](AsyncWebServerRequest* req) {
        SampleData s = dbManager.getLatestSample();
        String json = "{";
        json += "\"timestamp\":"    + String(s.timestamp)          + ",";
        json += "\"theta\":"        + String(s.theta, 4)           + ",";
        json += "\"psi_kpa\":"      + String(s.psi_kpa, 2)         + ",";
        json += "\"status\":\""     + s.status                     + "\",";
        json += "\"urgency\":\""    + s.urgency                    + "\",";
        json += "\"confidence\":"   + String(s.confidence, 2)      + ",";
        json += "\"theta_fc\":"     + String(activeCrop.theta_fc, 3)     + ",";
        json += "\"theta_refill\":" + String(activeCrop.theta_refill, 3) + ",";
        json += "\"stage\":\""      + activeCrop.stage_name        + "\",";
        json += "\"crop\":\""       + activeCrop.crop_key          + "\"";
        json += "}";
        req->send(200, "application/json", json);
    });

    server.on("/api/series", HTTP_GET, [](AsyncWebServerRequest* req) {
        long start = 0, end = 0;
        if (req->hasParam("start")) start = req->getParam("start")->value().toInt();
        if (req->hasParam("end"))   end   = req->getParam("end")->value().toInt();

        auto series = dbManager.getSamplesInRange(start, end);
        String json = "[";
        for (size_t i = 0; i < series.size(); i++) {
            if (i > 0) json += ",";
            json += "{\"timestamp\":" + String(series[i].timestamp) + ",";
            json += "\"theta\":"      + String(series[i].theta, 4)  + "}";
        }
        json += "]";
        req->send(200, "application/json", json);
    });

    server.onNotFound([](AsyncWebServerRequest* req) {
    req->redirect("http://192.168.4.1");
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

        int   raw  = analogRead(SOIL_PIN);
        tempSensor.requestTemperatures();
        float temp = tempSensor.getTempCByIndex(0);
        if (temp == DEVICE_DISCONNECTED_C) temp = 25.0f;

        time_t ts = time(nullptr);
        if (ts < 1000000) ts = seqTimestamp++;

        // Native C++ physics - no JS, no Duktape
        SensorReading reading = Physics.processSensorReading(raw, temp, ts);

        SampleData s;
        s.timestamp         = reading.timestamp;
        s.raw_adc           = reading.raw_adc;
        s.temp_c            = reading.temp_c;
        s.theta             = reading.theta;
        s.theta_fc          = reading.theta_fc;
        s.theta_refill      = reading.theta_refill;
        s.psi_kpa           = reading.psi_kPa;
        s.aw_mm             = reading.AW_mm;
        s.fraction_depleted = reading.fractionDepleted;
        s.drying_rate       = reading.dryingRate_per_hr;
        s.regime            = String(reading.regime);
        s.status            = String(reading.status);
        s.urgency           = String(reading.urgency);
        s.confidence        = reading.confidence;
        s.qc_valid          = reading.qc_valid;
        s.seq               = (int)(seqTimestamp - 1000000);

        Serial.printf("[SENSOR] theta=%.3f status=%s urgency=%s conf=%.2f\n",
                      reading.theta, reading.status,
                      reading.urgency, reading.confidence);

        sampleBuffer.push_back(s);
        if ((int)sampleBuffer.size() >= BATCH_SIZE) {
            dbManager.writeSampleBatch(sampleBuffer);
            sampleBuffer.clear();
            Serial.println("[DB] Batch flushed");
        }
    }
}
