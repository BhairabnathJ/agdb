#include "db_manager.h"
#include <FS.h>
#include <SD.h>

DBManager::DBManager(const char *path)
    : dbPath(path), db(nullptr), insertStmt(nullptr) {}

DBManager::~DBManager() {
  if (insertStmt)
    sqlite3_finalize(insertStmt);
  if (db)
    sqlite3_close(db);
}

bool DBManager::init() {
  // 1. Open Database
  int rc = sqlite3_open(dbPath, &db);
  if (rc != SQLITE_OK) {
    Serial.printf("DB Open Error: %s\n", sqlite3_errmsg(db));
    return false;
  }

  // 2. Enable WAL Mode (Critical for crash safety)
  executeSQL("PRAGMA journal_mode=WAL;");
  executeSQL("PRAGMA synchronous=NORMAL;"); // Faster writes, still safe in WAL

  // 3. Create Tables (Idempotent)
  const char *tableSQL =
      "CREATE TABLE IF NOT EXISTS samples ("
      "timestamp INTEGER PRIMARY KEY, raw_adc INTEGER, temp_c REAL, theta "
      "REAL, "
      "theta_fc REAL, theta_refill REAL, psi_kpa REAL, aw_mm REAL, "
      "fraction_depleted REAL, drying_rate REAL, regime TEXT, status TEXT, "
      "urgency TEXT, confidence REAL, qc_valid INTEGER, seq INTEGER);"
      "CREATE INDEX IF NOT EXISTS idx_timestamp ON samples(timestamp);"
      "CREATE TABLE IF NOT EXISTS calibration ("
      "version INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, state "
      "TEXT, "
      "theta_fc REAL, theta_refill REAL, n_events INTEGER, confidence REAL, "
      "params_json TEXT);";

  if (!executeSQL(tableSQL))
    return false;

  // 4. Prepare Statements
  return prepareStatements();
}

bool DBManager::prepareStatements() {
  const char *sql = "INSERT INTO samples VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                    "?, ?, ?, ?, ?, ?)";
  int rc = sqlite3_prepare_v2(db, sql, -1, &insertStmt, nullptr);
  if (rc != SQLITE_OK) {
    Serial.printf("Prepare Error: %s\n", sqlite3_errmsg(db));
    return false;
  }
  return true;
}

bool DBManager::writeSampleBatch(std::vector<SampleData> &samples) {
  if (samples.empty())
    return true;

  executeSQL("BEGIN TRANSACTION;");

  for (const auto &s : samples) {
    sqlite3_reset(insertStmt);

    sqlite3_bind_int64(insertStmt, 1, s.timestamp);
    sqlite3_bind_int(insertStmt, 2, s.raw_adc);
    sqlite3_bind_double(insertStmt, 3, s.temp_c);
    sqlite3_bind_double(insertStmt, 4, s.theta);
    sqlite3_bind_double(insertStmt, 5, s.theta_fc);
    sqlite3_bind_double(insertStmt, 6, s.theta_refill);
    sqlite3_bind_double(insertStmt, 7, s.psi_kpa);
    sqlite3_bind_double(insertStmt, 8, s.aw_mm);
    sqlite3_bind_double(insertStmt, 9, s.fraction_depleted);
    sqlite3_bind_double(insertStmt, 10, s.drying_rate);
    sqlite3_bind_text(insertStmt, 11, s.regime.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 12, s.status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 13, s.urgency.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(insertStmt, 14, s.confidence);
    sqlite3_bind_int(insertStmt, 15, s.qc_valid ? 1 : 0);
    sqlite3_bind_int(insertStmt, 16, s.seq);

    if (sqlite3_step(insertStmt) != SQLITE_DONE) {
      Serial.printf("Insert Step Error: %s\n", sqlite3_errmsg(db));
    }
  }

  executeSQL("COMMIT;");
  return true;
}

SampleData DBManager::getLatestSample() {
  SampleData s = {};
  sqlite3_stmt *stmt;
  const char *sql = "SELECT * FROM samples ORDER BY timestamp DESC LIMIT 1";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      s.timestamp = sqlite3_column_int64(stmt, 0);
      s.theta = sqlite3_column_double(stmt, 3);
      s.temp_c = sqlite3_column_double(stmt, 2);
      s.status = String((const char *)sqlite3_column_text(stmt, 11));
      s.confidence = sqlite3_column_double(stmt, 13);
      s.psi_kpa = sqlite3_column_double(stmt, 6);
      s.aw_mm = sqlite3_column_double(stmt, 7);
      s.urgency = String((const char *)sqlite3_column_text(stmt, 12));
    }
  }
  sqlite3_finalize(stmt);
  return s;
}

std::vector<SampleData> DBManager::getSamplesInRange(time_t start, time_t end) {
  std::vector<SampleData> res;
  sqlite3_stmt *stmt;
  // Limit to 200 points to prevent memory overflow on ESP32
  const char *sql = "SELECT timestamp, theta FROM samples WHERE timestamp "
                    "BETWEEN ? AND ? ORDER BY timestamp ASC LIMIT 200";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, start);
    sqlite3_bind_int64(stmt, 2, end);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      SampleData s = {};
      s.timestamp = sqlite3_column_int64(stmt, 0);
      s.theta = sqlite3_column_double(stmt, 1);
      res.push_back(s);
    }
  }
  sqlite3_finalize(stmt);
  return res;
}

bool DBManager::executeSQL(const char *sql) {
  char *errMsg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("SQL Error: %s\n", errMsg);
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

String DBManager::getCalibrationJSON() {
  // Stub
  return "{}";
}

bool DBManager::writeCalibration(String state, float fc, float refill,
                                 int n_events, float conf, String params_json) {
  return true; // Stub
}

bool DBManager::cleanOldData(int daysToKeep) {
  return true; // Stub
}
