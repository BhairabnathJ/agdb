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

// =============================================================================
// SCHEMA MIGRATION
// =============================================================================

struct ColDef {
  const char *name;
  const char *definition; // type + DEFAULT clause, e.g. "REAL DEFAULT -1"
};

// Single-pass migration: reads PRAGMA table_info once, then ALTER TABLEs any
// column from `cols` that is absent.  Safe to re-run on every boot.
// Fresh databases created with the full CREATE TABLE will have all columns
// already present, so no ALTERs will run.  Old databases get each missing
// column added one at a time as a fallback.
// Error handling:
//   - "duplicate column" → silent (PRAGMA check should prevent this, but
//     handle it defensively without alarming the log)
//   - any other failure (including parser stack overflow) → log warning and
//     continue; never blocks boot
static void migrateTable(sqlite3 *db, const char *table, const ColDef *cols,
                         int nCols) {
  // Cap at 32 to accommodate future column additions without code changes
  static const int MAX_COLS = 32;
  bool found[MAX_COLS] = {};
  int check = nCols < MAX_COLS ? nCols : MAX_COLS;

  char pragma[64];
  snprintf(pragma, sizeof(pragma), "PRAGMA table_info(%s)", table);

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, pragma, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *cn = (const char *)sqlite3_column_text(stmt, 1);
      if (!cn)
        continue;
      for (int i = 0; i < check; i++) {
        if (strcmp(cn, cols[i].name) == 0) {
          found[i] = true;
          break;
        }
      }
    }
    sqlite3_finalize(stmt);
  }

  for (int i = 0; i < check; i++) {
    if (found[i])
      continue;
    char sql[128];
    snprintf(sql, sizeof(sql), "ALTER TABLE %s ADD COLUMN %s %s", table,
             cols[i].name, cols[i].definition);
    char *err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc == SQLITE_OK) {
      Serial.printf("[DB] %s: added column %s\n", table, cols[i].name);
    } else {
      // "duplicate column name" is harmless — the column already exists
      // despite not appearing in PRAGMA (can happen on corrupt table_info).
      // Anything else (stack overflow, locked DB, etc.) is unexpected but
      // must not block boot — log and move on.
      const char *msg = err ? err : sqlite3_errmsg(db);
      if (strstr(msg, "duplicate column") != nullptr) {
        // column already present — silent
      } else {
        Serial.printf("[DB] WARN: %s + %s failed: %s — skipping\n", table,
                      cols[i].name, msg);
      }
      if (err)
        sqlite3_free(err);
    }
  }
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
      "urgency TEXT, confidence REAL, qc_valid INTEGER, seq INTEGER, "
      "air_temp_c REAL DEFAULT -1, humidity REAL DEFAULT -1, "
      "raw_adc_2 INTEGER DEFAULT -1, theta_2 REAL DEFAULT -1, "
      "device_id TEXT DEFAULT 'HUB_ONBOARD', battery_pct INTEGER DEFAULT -1);"
      "CREATE INDEX IF NOT EXISTS idx_timestamp ON samples(timestamp);"
      "CREATE TABLE IF NOT EXISTS calibration ("
      "version INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, state "
      "TEXT, "
      "theta_fc REAL, theta_refill REAL, n_events INTEGER, confidence REAL, "
      "params_json TEXT);";

  if (!executeSQL(tableSQL))
    return false;

  // 3b. Schema migration — fallback for databases created before the current
  //     schema.  Fresh databases already have all columns from CREATE TABLE
  //     above, so migrateTable will find every column present and run zero
  //     ALTERs.  Old databases get only the missing columns added.
  //     ALTER failures are non-fatal; see migrateTable() for details.
  static const ColDef samplesCols[] = {
      {"timestamp", "INTEGER NOT NULL DEFAULT 0"},
      {"raw_adc", "INTEGER DEFAULT 0"},
      {"temp_c", "REAL DEFAULT 0"},
      {"theta", "REAL DEFAULT 0"},
      {"theta_fc", "REAL DEFAULT 0"},
      {"theta_refill", "REAL DEFAULT 0"},
      {"psi_kpa", "REAL DEFAULT 0"},
      {"aw_mm", "REAL DEFAULT 0"},
      {"fraction_depleted", "REAL DEFAULT 0"},
      {"drying_rate", "REAL DEFAULT 0"},
      {"regime", "TEXT DEFAULT ''"},
      {"status", "TEXT DEFAULT ''"},
      {"urgency", "TEXT DEFAULT ''"},
      {"confidence", "REAL DEFAULT 0"},
      {"qc_valid", "INTEGER DEFAULT 0"},
      {"seq", "INTEGER DEFAULT 0"},
      {"air_temp_c", "REAL DEFAULT -1"},
      {"humidity", "REAL DEFAULT -1"},
      {"raw_adc_2", "INTEGER DEFAULT -1"},
      {"theta_2", "REAL DEFAULT -1"},
      {"device_id", "TEXT DEFAULT 'HUB_ONBOARD'"},
      {"battery_pct", "INTEGER DEFAULT -1"},
  };
  migrateTable(db, "samples", samplesCols,
               sizeof(samplesCols) / sizeof(samplesCols[0]));

  static const ColDef calibCols[] = {
      {"timestamp", "INTEGER DEFAULT 0"}, {"state", "TEXT DEFAULT ''"},
      {"theta_fc", "REAL DEFAULT 0"},     {"theta_refill", "REAL DEFAULT 0"},
      {"n_events", "INTEGER DEFAULT 0"},  {"confidence", "REAL DEFAULT 0"},
      {"params_json", "TEXT DEFAULT ''"},
  };
  migrateTable(db, "calibration", calibCols,
               sizeof(calibCols) / sizeof(calibCols[0]));

  // 4. Prepare Statements
  return prepareStatements();
}

bool DBManager::prepareStatements() {
  // Explicitly list columns — id is excluded, SQLite fills it automatically
  const char *sql =
      "INSERT INTO samples "
      "(timestamp, raw_adc, temp_c, theta, theta_fc, theta_refill, "
      "psi_kpa, aw_mm, fraction_depleted, drying_rate, regime, "
      "status, urgency, confidence, qc_valid, seq, air_temp_c, humidity, "
      "raw_adc_2, theta_2, device_id, battery_pct) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
      "?)";

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
    // regime=11, status=12, urgency=13, confidence=14, qc_valid=15, seq=16,
    // air_temp_c=17
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
    sqlite3_bind_double(insertStmt, 17, s.air_temp_c);
    sqlite3_bind_double(insertStmt, 18, s.humidity);
    sqlite3_bind_int(insertStmt, 19, s.raw_adc_2);
    sqlite3_bind_double(insertStmt, 20, s.theta_2);
    sqlite3_bind_text(insertStmt, 21, s.device_id.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(insertStmt, 22, s.battery_pct);

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
      // col 15: qc_valid, col 16: seq, col 17: air_temp_c
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
      s.air_temp_c = sqlite3_column_double(stmt, 17);
    }
  }
  sqlite3_finalize(stmt);
  return s;
}

std::vector<SampleData> DBManager::getRecentSamples(int n) {
  std::vector<SampleData> res;
  sqlite3_stmt *stmt;
  const char *sql =
      "SELECT id, timestamp, raw_adc, temp_c, theta, theta_fc, theta_refill, "
      "psi_kpa, aw_mm, fraction_depleted, drying_rate, regime, status, "
      "urgency, confidence, qc_valid, seq, air_temp_c, humidity, "
      "raw_adc_2, theta_2, device_id, battery_pct "
      "FROM samples ORDER BY timestamp DESC LIMIT ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      SampleData s = {};
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
      s.air_temp_c = sqlite3_column_double(stmt, 17);
      s.humidity = sqlite3_column_double(stmt, 18);
      s.raw_adc_2 = sqlite3_column_int(stmt, 19);
      s.theta_2 = sqlite3_column_double(stmt, 20);
      s.device_id = String((const char *)sqlite3_column_text(stmt, 21));
      s.battery_pct = sqlite3_column_int(stmt, 22);
      res.push_back(s);
    }
  }
  sqlite3_finalize(stmt);
  return res;
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

SampleData DBManager::getLatestSampleForDevice(const String &deviceId) {
  SampleData s = {};
  sqlite3_stmt *stmt;
  const char *sql =
      "SELECT id, timestamp, raw_adc, temp_c, theta, theta_fc, theta_refill, "
      "psi_kpa, aw_mm, fraction_depleted, drying_rate, regime, status, "
      "urgency, confidence, qc_valid, seq, air_temp_c, humidity, "
      "raw_adc_2, theta_2, device_id, battery_pct "
      "FROM samples WHERE device_id = ? ORDER BY timestamp DESC LIMIT 1";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
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
      s.air_temp_c = sqlite3_column_double(stmt, 17);
      s.humidity = sqlite3_column_double(stmt, 18);
      s.raw_adc_2 = sqlite3_column_int(stmt, 19);
      s.theta_2 = sqlite3_column_double(stmt, 20);
      s.device_id = String((const char *)sqlite3_column_text(stmt, 21));
      s.battery_pct = sqlite3_column_int(stmt, 22);
    }
  }
  sqlite3_finalize(stmt);
  return s;
}

std::vector<SampleData> DBManager::getRecentSamples(int n,
                                                    const String &deviceId) {
  std::vector<SampleData> res;
  sqlite3_stmt *stmt;
  const char *sql =
      "SELECT id, timestamp, raw_adc, temp_c, theta, theta_fc, theta_refill, "
      "psi_kpa, aw_mm, fraction_depleted, drying_rate, regime, status, "
      "urgency, confidence, qc_valid, seq, air_temp_c, humidity, "
      "raw_adc_2, theta_2, device_id, battery_pct "
      "FROM samples WHERE device_id = ? ORDER BY timestamp DESC LIMIT ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      SampleData s = {};
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
      s.air_temp_c = sqlite3_column_double(stmt, 17);
      s.humidity = sqlite3_column_double(stmt, 18);
      s.raw_adc_2 = sqlite3_column_int(stmt, 19);
      s.theta_2 = sqlite3_column_double(stmt, 20);
      s.device_id = String((const char *)sqlite3_column_text(stmt, 21));
      s.battery_pct = sqlite3_column_int(stmt, 22);
      res.push_back(s);
    }
  }
  sqlite3_finalize(stmt);
  return res;
}

time_t DBManager::getDeviceLastSeen(const String &deviceId) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT MAX(timestamp) FROM samples WHERE device_id = ?";
  time_t result = 0;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
      result = (time_t)sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return result;
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