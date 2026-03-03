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
  executeSQL("PRAGMA synchronous=NORMAL;");

  // 3. Create Tables
  // NOTE: id is AUTOINCREMENT primary key — timestamp is NOT unique
  // Multiple readings per second are allowed (sensor loop runs fast)
  const char *tableSQL =
      "CREATE TABLE IF NOT EXISTS samples ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "timestamp INTEGER NOT NULL, "
      "raw_adc INTEGER, temp_c REAL, theta REAL, "
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
  // Explicitly list columns — id is excluded, SQLite fills it automatically
  const char *sql =
      "INSERT INTO samples "
      "(timestamp, raw_adc, temp_c, theta, theta_fc, theta_refill, "
      "psi_kpa, aw_mm, fraction_depleted, drying_rate, regime, "
      "status, urgency, confidence, qc_valid, seq) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

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

    // Binds map to: timestamp=1, raw_adc=2, temp_c=3, theta=4, theta_fc=5,
    // theta_refill=6, psi_kpa=7, aw_mm=8, fraction_depleted=9, drying_rate=10,
    // regime=11, status=12, urgency=13, confidence=14, qc_valid=15, seq=16
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
      // Column offsets shifted by 1 due to id being column 0
      // col 0: id, col 1: timestamp, col 2: raw_adc, col 3: temp_c,
      // col 4: theta, col 5: theta_fc, col 6: theta_refill, col 7: psi_kpa,
      // col 8: aw_mm, col 9: fraction_depleted, col 10: drying_rate,
      // col 11: regime, col 12: status, col 13: urgency, col 14: confidence,
      // col 15: qc_valid, col 16: seq
      s.id = sqlite3_column_int(stmt, 0);
      s.timestamp = sqlite3_column_int64(stmt, 1);
      s.raw_adc = sqlite3_column_int(stmt, 2);
      s.temp_c = sqlite3_column_double(stmt, 3);
      s.theta = sqlite3_column_double(stmt, 4);
      s.theta_fc = sqlite3_column_double(stmt, 5);
      s.theta_refill = sqlite3_column_double(stmt, 6);
      s.psi_kpa = sqlite3_column_double(stmt, 7);
      s.aw_mm = sqlite3_column_double(stmt, 8);
      s.fraction_depleted = sqlite3_column_double(stmt, 9);
      s.drying_rate = sqlite3_column_double(stmt, 10);
      s.regime = String((const char *)sqlite3_column_text(stmt, 11));
      s.status = String((const char *)sqlite3_column_text(stmt, 12));
      s.urgency = String((const char *)sqlite3_column_text(stmt, 13));
      s.confidence = sqlite3_column_double(stmt, 14);
      s.qc_valid = sqlite3_column_int(stmt, 15) != 0;
      s.seq = sqlite3_column_int(stmt, 16);
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