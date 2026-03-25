#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <Arduino.h>
#include <sqlite3.h>
#include <vector>

struct SampleData {
  int id; // AUTOINCREMENT primary key — do not set manually
  time_t timestamp;
  int raw_adc;
  int raw_adc_2 = -1;
  float temp_c;
  float humidity = -1.0f;
  float air_temp_c = -1.0f;
  float theta;
  float theta_2 = -1.0f;
  float theta_fc;
  float theta_refill;
  float psi_kpa;
  float aw_mm;
  float fraction_depleted;
  float drying_rate;
  String regime;
  String status;
  String urgency;
  float confidence;
  bool qc_valid;
  int seq;
  String device_id = "HUB_ONBOARD";
  int battery_pct = -1;
};

class DBManager {
public:
  DBManager(const char *dbPath);
  ~DBManager();

  // Lifecycle
  bool init();

  // Core Operations
  bool writeSampleBatch(std::vector<SampleData> &samples);
  SampleData getLatestSample();
  std::vector<SampleData> getRecentSamples(int n);
  std::vector<SampleData> getSamplesInRange(time_t start, time_t end);
  SampleData getLatestSampleForDevice(const String &deviceId);
  std::vector<SampleData> getRecentSamples(int n, const String &deviceId);
  time_t getDeviceLastSeen(const String &deviceId);

  // Calibration
  bool writeCalibration(String state, float fc, float refill, int n_events,
                        float conf, String params_json);
  String getCalibrationJSON();

  // Maintenance
  bool cleanOldData(int daysToKeep);

private:
  sqlite3 *db;
  const char *dbPath;
  sqlite3_stmt *insertStmt;

  bool executeSQL(const char *sql);
  bool prepareStatements();
};

#endif