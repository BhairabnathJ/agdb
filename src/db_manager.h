#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <sqlite3.h>
#include <vector>
#include <Arduino.h> // Assuming Arduino framework for String/Serial

struct SampleData {
    time_t timestamp;
    int raw_adc;
    float temp_c;
    float theta;
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
};

class DBManager {
public:
    DBManager(const char* dbPath);
    ~DBManager();
    
    // Lifecycle
    bool init();
    
    // Core Operations
    bool writeSampleBatch(std::vector<SampleData>& samples);
    SampleData getLatestSample();
    std::vector<SampleData> getRecentSamples(int n);
    std::vector<SampleData> getSamplesInRange(time_t start, time_t end);
    
    // Calibration
    bool writeCalibration(String state, float fc, float refill, int n_events, float conf, String params_json);
    String getCalibrationJSON();
    
    // Maintenance
    bool cleanOldData(int daysToKeep);
    
private:
    sqlite3* db;
    const char* dbPath;
    sqlite3_stmt* insertStmt;
    
    bool executeSQL(const char* sql);
    bool prepareStatements();
};

#endif
