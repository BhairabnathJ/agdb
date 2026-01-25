-- AgriScan SQLite Schema
-- Target: /sd/agriscan.db

-- 1. Main sensor readings + physics calculations
CREATE TABLE IF NOT EXISTS samples (
    timestamp INTEGER PRIMARY KEY,  -- Unix timestamp (seconds)
    raw_adc INTEGER,                -- Raw sensor value
    temp_c REAL,                    -- Temperature (°C)
    theta REAL,                     -- Volumetric water content (m³/m³)
    theta_fc REAL,                  -- Auto-calibrated field capacity
    theta_refill REAL,              -- Auto-calibrated refill point
    psi_kpa REAL,                   -- Matric potential (kPa)
    aw_mm REAL,                     -- Available water (mm)
    fraction_depleted REAL,         -- Depletion (0-1)
    drying_rate REAL,               -- dθ/dt (m³/m³/hr)
    regime TEXT,                    -- 'wetting'|'drainage'|'drydown'|'stable'
    status TEXT,                    -- 'REFILL'|'MONITOR'|'OPTIMAL'|'FULL'
    urgency TEXT,                   -- 'none'|'low'|'medium'|'high'
    confidence REAL,                -- Calibration confidence (0-1)
    qc_valid INTEGER,               -- 0/1 boolean
    seq INTEGER                     -- Sequence number
);

CREATE INDEX IF NOT EXISTS idx_timestamp ON samples(timestamp);

-- 2. Detected events (wetting/drying)
CREATE TABLE IF NOT EXISTS events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_start INTEGER,
    ts_end INTEGER,
    event_type TEXT,    -- 'wetting'|'drainage'|'drydown'
    delta_theta REAL,
    metadata_json TEXT
);

-- 3. Calibration history
CREATE TABLE IF NOT EXISTS calibration (
    version INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER,
    state TEXT,         -- State machine state
    theta_fc REAL,
    theta_refill REAL,
    n_events INTEGER,
    confidence REAL,
    params_json TEXT    -- Full dynamics parameters as JSON
);
