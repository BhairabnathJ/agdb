# AgriScan Codebase Integration Review

**Date:** 2026-01-24
**Status:** ISSUES FOUND - Requires Fixes Before Deployment

---

## Summary

The codebase has solid individual components but **critical integration issues** prevent it from working as intended. The physics engine is well-implemented, the tiered UI structure is correct, and the C++ database layer is functional. However, field name mismatches, missing HTML elements, and file path discrepancies will cause runtime errors.

---

## Component Status

| Component | File(s) | Status | Issues |
|-----------|---------|--------|--------|
| Dashboard HTML | `index.html` | ⚠️ Partial | Missing 8 element IDs referenced by JS |
| Stylesheet | `css/main.css` | ⚠️ Partial | Font sizes below 14px minimum |
| App Logic | `js/app.js` | ⚠️ Partial | Field name mismatches with physics.js |
| Physics Engine | `js/physics.js` | ✅ Good | Well-structured, comprehensive |
| I18n | `js/i18n.js` | ✅ Good | Works as expected |
| DB Schema | `schema.sql` | ✅ Good | Properly defined |
| DB Manager | `src/db_manager.cpp` | ⚠️ Partial | Stub functions, incomplete field extraction |
| ESP32 Main | `src/main.cpp` | ⚠️ Partial | File paths wrong, incomplete field extraction |

---

## Critical Issues (Must Fix)

### 1. Field Name Mismatches Between physics.js and app.js

The physics engine returns camelCase fields, but app.js expects snake_case in some places:

| Physics.js Returns | app.js Expects | Location |
|--------------------|----------------|----------|
| `psi_kPa` | `psi_kpa` | renderTier3() line 269 |
| `AW_mm` | `aw_mm` | renderTier3() line 271 |
| `fractionDepleted` | `fraction_depleted` | renderTier3() line 272 |
| `dryingRate_per_hr` | `drying_rate` | renderTier3() line 276 |
| `raw` | `raw_adc` | MockAPI and multiple locations |

**Impact:** Tier 3 displays will show `--` or `undefined` for these values.

**Fix Required:** Either update physics.js to use snake_case, or update app.js to read camelCase fields.

---

### 2. Missing HTML Element IDs

app.js references these IDs in `renderTier3()` and `setupListeners()`, but they don't exist in index.html:

```
t3-air-temp     - line 267
t3-humid        - line 268
t3-balance      - line 273
t3-drying       - line 276
t3-et0          - line 277
t3-risk         - line 278
t3-reason-text  - line 281
t3-zone-select  - line 369
```

**Impact:** `setSafeText()` silently fails, but `setupListeners()` will throw errors on `t3-zone-select`.

**Fix Required:** Either add these elements to index.html or remove the references from app.js.

---

### 3. ESP32 File Path Discrepancies

In `main.cpp`:

```cpp
// Line 32 - Loads physics from wrong path
File f = SD.open("/physics.js");
// Should be: SD.open("/js/physics.js");

// Line 113 - Serves static files from /www/ subdirectory
server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");
// But actual files are in root: /index.html, /css/, /js/
```

**Current file structure:**
```
/sd/
├── index.html          ← In root
├── css/main.css        ← In /css/
├── js/app.js           ← In /js/
├── js/physics.js       ← In /js/
└── agriscan.db
```

**Expected by main.cpp:**
```
/sd/
├── www/
│   ├── index.html      ← In /www/
│   ├── css/main.css
│   └── js/app.js
├── physics.js          ← In root
└── agriscan.db
```

**Impact:** 404 errors for all static files when running on ESP32.

**Fix Required:** Either reorganize files on SD card, or update main.cpp paths.

---

### 4. Incomplete C++ Field Extraction

`db_manager.cpp` `getLatestSample()` only extracts 8 of 16 fields:

```cpp
// Currently extracts:
timestamp, theta, temp_c, status, confidence, psi_kpa, aw_mm, urgency

// Missing:
raw_adc, theta_fc, theta_refill, fraction_depleted, drying_rate, regime, qc_valid, seq
```

`main.cpp` `runPhysics()` only extracts 5 fields from JS engine:

```cpp
// Currently extracts:
theta, status, psi_kpa, aw_mm, confidence

// Missing:
theta_fc, theta_refill, fraction_depleted, drying_rate, regime, urgency, qc_valid
```

**Impact:** Database stores partial data, API returns incomplete records.

---

### 5. Events Table Not Created

`schema.sql` defines an `events` table, but `db_manager.cpp` doesn't create it:

```sql
-- In schema.sql but NOT in db_manager.cpp:
CREATE TABLE IF NOT EXISTS events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_start INTEGER,
    ts_end INTEGER,
    event_type TEXT,
    delta_theta REAL,
    metadata_json TEXT
);
```

**Impact:** Event detection data cannot be persisted.

---

## Moderate Issues (Should Fix)

### 6. CSS Font Sizes Below Specification Minimum

From specification-v2.md: "No text smaller than 14px anywhere"

Current violations:

| Selector | Current Size | Required |
|----------|--------------|----------|
| `.metric-cell label` | 10px | 14px |
| `.stat-label` | 11px | 14px |
| `.hero-footer` | 12px | 14px |

---

### 7. Color Palette Not Updated

CSS uses pure white and cool grays instead of warm palette from spec:

```css
/* Current */
--white: #FFF;
--neutral-100: #F7F7F7;

/* Specification v2 */
--white: #FFFDF7;        /* Warm white */
--neutral-100: #F5F3EF;  /* Warm off-white */
```

---

### 8. Tier 2 Dynamic Header

The "Why do I need to water?" heading is hardcoded, but should change based on status:

- If status is REFILL: "Why do I need to water?"
- If status is MONITOR: "Why should I check the field?"
- If status is OPTIMAL/FULL: "Why is everything good?"

---

### 9. MockAPI vs Real API

`app.js` uses a MockAPI for browser testing:

```javascript
const MockAPI = {
    getCurrent: async function() { ... },
    getSeries: async function() { ... }
}
```

For ESP32 deployment, this needs to be replaced with:

```javascript
const API = {
    getCurrent: async function() {
        const res = await fetch('/api/current');
        return res.json();
    },
    getSeries: async function(start, end) {
        const res = await fetch(`/api/series?start=${start}&end=${end}`);
        return res.json();
    }
}
```

---

### 10. Stub Functions in db_manager.cpp

These functions are stubs and do nothing:

```cpp
String DBManager::getCalibrationJSON() { return "{}"; }  // Line 149
bool DBManager::writeCalibration(...) { return true; }   // Line 154
bool DBManager::cleanOldData(...) { return true; }       // Line 159
```

Calibration history won't be saved, and old data won't be cleaned up.

---

## Minor Issues (Nice to Fix)

### 11. Missing getRecentSamples() Implementation

`db_manager.h` declares `getRecentSamples(int n)` but it's not implemented in the .cpp file.

### 12. Hardcoded WiFi Credentials

```cpp
WiFi.softAP("AgriScan_Connect", "agri1234");
```

Should be configurable via settings.

### 13. No Settings API Endpoint

Settings page saves to localStorage, but there's no `/api/settings` endpoint on ESP32 to persist configuration.

### 14. Chart Tooltip Not Implemented

The specification calls for touch-to-inspect tooltips on charts, but the SVG chart in app.js has no touch event handlers.

---

## Recommended Fix Order

### Phase 1: Critical (Blocking Issues)

1. **Fix field name mismatches** - Update physics.js to return snake_case fields OR update app.js to read camelCase
2. **Add missing HTML IDs** - Add elements or remove dead references
3. **Fix file paths in main.cpp** - Match actual file structure
4. **Complete field extraction in C++** - Extract all 16 fields

### Phase 2: Functional (Required for Full Feature Set)

5. Add events table creation to db_manager.cpp
6. Implement stub functions (getCalibrationJSON, writeCalibration, cleanOldData)
7. Implement getRecentSamples()
8. Add real API calls (replace MockAPI for production)

### Phase 3: Polish (Specification Compliance)

9. Update CSS font sizes to 14px minimum
10. Apply warm color palette
11. Dynamic Tier 2 headers
12. Add settings API endpoint

---

## Integration Test Checklist

After fixes, verify:

- [ ] Tier 1 displays status icon with correct color
- [ ] Tier 1 message updates based on urgency
- [ ] Tier 2 reasoning list populates correctly
- [ ] Tier 3 shows all 6 metrics with values (not --)
- [ ] Tier 3 chart renders with data points
- [ ] Simulate Rain button increases VWC
- [ ] Simulate Drought button decreases VWC
- [ ] CSV export downloads file with data
- [ ] Settings save and persist
- [ ] Language toggle works (EN/ES)

---

## Conclusion

The architecture is sound and the physics engine is well-implemented. The primary issues are **integration mismatches** between components that were likely developed separately. With the fixes outlined above, the system should work as intended.

**Estimated fix effort:** 2-4 hours for critical issues, additional 2-3 hours for moderate issues.
