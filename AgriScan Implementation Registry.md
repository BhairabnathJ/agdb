**AgriScan**

**Implementation Registry for Claude Code**

Version 2.0  |  March 2026  |  Alex Jamkatel, CTO  |  Unity Provisions

Purpose: Authoritative implementation guide for Claude Code. Each issue includes exact file locations, what to remove, what to replace it with, and verification steps.

# **How to Use This Document**

This document is the single source of truth for all codebase changes. Each issue is a self-contained implementation task. Work through them in priority order — P0 first, then P1, then P2. Do not skip P0 items as later fixes depend on the architecture they establish.

For each issue you will find:

* ROOT CAUSE — exactly what is wrong and why

* FILES — every file that needs to change

* REMOVE — the exact code block or pattern to delete

* IMPLEMENT — the replacement code pattern with full logic

* VERIFY — how to confirm the fix is working correctly

# **Issue Summary**

| \# | Issue | Severity | File(s) | One-Line Impact |
| :---- | :---- | :---- | :---- | :---- |
| 1 | Single global Physics instance | physics.js, app.js | All zones share one calibration state — wrong recommendations for every zone |  |
| 2 | No per-device state persistence | firmware/main.ino, app.js | Reboot wipes all calibration progress — system restarts from zero every power cycle |  |
| 3 | Mock data never cleared on boot | app.js | Dashboard always shows fake random data — real sensor readings are invisible |  |
| 4 | Legacy stub functions returning constants | physics.js | VPD, ET0, drying rate, time-to-critical all return hardcoded wrong values |  |
| 5 | History cap mismatch 200 vs 2880 | app.js | Auto-calibration starved of data — zone history lost 14x faster than physics expects |  |
| 6 | No mock vs real hardware toggle | app.js | No clean path to real hardware — MockAPI and RealAPI tangled, deployment blocked |  |
| 7 | No CropBand pairing system | firmware/main.ino, app.js | ESP-NOW receive handler not implemented — no data from CropBands possible |  |
| 8 | Hardcoded zone map ZONE\_CONFIG | app.js | Zone map shows 16 fake zones regardless of what is actually paired |  |
| 9 | Onboarding not writing correct fields | onboarding.html, app.js | EXTERNAL\_CONFIG stays null — physics engine silently uses wrong soil and crop |  |
| 10 | Flow rate required not optional | onboarding.html, app.js | Farmers who do not know flow rate cannot complete setup |  |
| 11 | No ADC sensor health validation | firmware/main.ino | Stuck or disconnected sensors not flagged — bad data enters physics engine |  |
| 12 | Settings saved to browser localStorage | app.js | Clearing browser loses all settings — Hub SD card is the source of truth |  |
| 13 | No SD card storage management UI | app.js, firmware/main.ino | Storage fills silently with no warning or way to clear from dashboard |  |
| 14 | Desktop dev dashboard not built | app.js, index.html | No live graph, no calibration state panel, no event log, no dev tools in UI |  |
| 15 | Double script loading | index.html | physics.js and i18n.js potentially loaded twice — race conditions, wasted RAM |  |
| 16 | Hardcoded zone label names | app.js | Zone names like North Orchard are fiction — not tied to real farm layout |  |

# **P0 — Critical: Fix Before Any Field Use**

These four issues must be resolved before the system is used on real hardware. They are foundational — everything else depends on them being correct.

## **Issue 1 — Single Global Physics Instance**

**ROOT CAUSE**

At the bottom of physics.js, the adapter block instantiates one single PhysicsEngine and assigns it to window.Physics. Every call to Physics.processSensorReading() regardless of which zone or device is calling it runs through this one shared instance. The AutoCalibration state machine inside it is accumulating events from all zones mixed together. Field Capacity learned from Zone A soil is being applied to Zone B which may be completely different soil.

**FILES TO CHANGE**

* js/physics.js — the adapter block at the bottom of the file

* js/app.js — every call site that uses the global Physics object

**LOCATE IN physics.js — find and REMOVE this entire block:**

// ADAPTER FOR FIRMWARE COMPATIBILITY

if (typeof window \!== 'undefined' && window.AgriScanPhysics) {

    window.Physics \= new window.AgriScanPhysics.PhysicsEngine();

    window.Physics.calculateVPD \= function (t, h) { return 0.5; };

    window.Physics.calculateET0 \= function (t) { return 4.5; };

    window.Physics.calculateDryingRate \= function (m, t) { return 0.2; };

    window.Physics.decideAction \= function (m, c, w) { return 'ALL\_GOOD'; };

    window.Physics.calculateTimeToCritical \= function (m, c, r) { return 12; };

}

**REPLACE WITH — add this new PhysicsRegistry object in its place:**

if (typeof window \!== 'undefined' && window.AgriScanPhysics) {

  window.PhysicsRegistry \= {

    \_instances: {},

    getOrCreate: function(deviceId) {

      if (\!this.\_instances\[deviceId\]) {

        this.\_instances\[deviceId\] \=

          new window.AgriScanPhysics.PhysicsEngine();

        console.log('\[PhysicsRegistry\] New instance for', deviceId);

      }

      return this.\_instances\[deviceId\];

    },

    remove: function(deviceId) {

      delete this.\_instances\[deviceId\];

    },

    listIds: function() {

      return Object.keys(this.\_instances);

    }

  };

  // Keep window.Physics as the HUB onboard sensor instance only

  window.Physics \= window.PhysicsRegistry.getOrCreate('HUB\_ONBOARD');

}

**UPDATE in app.js — MockAPI.inject() currently calls Physics directly. Change it to:**

inject: function(raw\_adc, temp\_c, zoneId \= 'HUB\_ONBOARD') {

  const engine \= window.PhysicsRegistry.getOrCreate(zoneId);

  const sample \= engine.processSensorReading(raw\_adc, temp\_c, ts);

  // rest of inject logic unchanged

}

**VERIFY**

* Open browser console after loading dashboard

* You should see '\[PhysicsRegistry\] New instance for HUB\_ONBOARD' on load

* Call PhysicsRegistry.listIds() in console — should return \['HUB\_ONBOARD'\]

* Inject data for two different zoneIds — listIds() should show both

* Confirm each zone shows different confidence scores over time

## **Issue 2 — No Per-Device State Persistence**

**ROOT CAUSE**

When the ESP32 reboots, the Duktape JS context is destroyed and recreated. The PhysicsEngine instance and all its AutoCalibration state — theta\_fc\_star, theta\_refill\_star, n\_events, confidence, current state machine stage — is lost entirely. A device that spent 7 days learning field capacity restarts from INIT as if installed yesterday. This is in firmware/main.ino.

**FILES TO CHANGE**

* firmware/main.ino — add save and load functions for calibration state

* js/physics.js — add exportState() and importState() methods to PhysicsEngine

**ADD to PhysicsEngine class in physics.js — two new methods:**

exportState() {

  return {

    version: 1,

    theta\_fc\_star: this.autoCalibration.theta\_fc\_star,

    theta\_refill\_star: this.autoCalibration.theta\_refill\_star,

    confidence: this.autoCalibration.confidence,

    n\_events: this.autoCalibration.stats.n\_events,

    n\_fc\_updates: this.autoCalibration.stats.n\_fc\_updates,

    cal\_state: this.autoCalibration.state,

    kd: this.dynamicsModel.params.kd,

    ku: this.dynamicsModel.params.ku,

    beta: this.dynamicsModel.params.beta,

    saved\_at: Math.floor(Date.now() / 1000\)

  };

}

importState(s) {

  if (\!s || s.version \!== 1\) return false;

  this.autoCalibration.theta\_fc\_star    \= s.theta\_fc\_star;

  this.autoCalibration.theta\_refill\_star \= s.theta\_refill\_star;

  this.autoCalibration.confidence       \= s.confidence;

  this.autoCalibration.stats.n\_events   \= s.n\_events;

  this.autoCalibration.stats.n\_fc\_updates \= s.n\_fc\_updates;

  this.autoCalibration.state            \= s.cal\_state;

  this.dynamicsModel.params.kd          \= s.kd;

  this.dynamicsModel.params.ku          \= s.ku;

  this.dynamicsModel.params.beta        \= s.beta;

  return true;

}

**ADD to firmware/main.ino — two C++ functions. Call saveCalibration() after every runPhysics(). Call loadCalibration() in setup() after setupJS():**

void saveCalibration(const String& deviceMac) {

  String call \= "JSON.stringify(Physics.exportState())";

  if (duk\_peval\_string(ctx, call.c\_str()) \== 0\) {

    String json \= duk\_safe\_to\_string(ctx, \-1);

    String path \= "/calibration/" \+ deviceMac \+ ".json";

    File f \= SD.open(path, FILE\_WRITE);

    if (f) { f.print(json); f.close(); }

  }

  duk\_pop(ctx);

}

void loadCalibration(const String& deviceMac) {

  String path \= "/calibration/" \+ deviceMac \+ ".json";

  File f \= SD.open(path, FILE\_READ);

  if (\!f) return;

  String json \= f.readString(); f.close();

  json.replace("\\"", "\\\\\\"");

  String call \= "Physics.importState(JSON.parse(\\"" \+ json \+ "\\"))";

  duk\_peval\_string(ctx, call.c\_str());

  duk\_pop(ctx);

  Serial.println("\[CAL\] Restored calibration for " \+ deviceMac);

}

**VERIFY**

* Flash firmware and let Hub run for 5+ minutes to accumulate calibration data

* Check SD card — /calibration/HUB\_ONBOARD.json should exist and contain non-null theta\_fc\_star

* Reboot the Hub

* On serial monitor you should see \[CAL\] Restored calibration for HUB\_ONBOARD

* Confidence score in dashboard should match pre-reboot value, not reset to zero

## **Issue 3 — Mock Data Never Cleared on Boot**

**ROOT CAUSE**

In app.js, the App.init() function always calls MockAPI.init() and MockAPI.seed(). seed() generates 30 fake readings per zone using random ADC values between 500-800 and random temperatures. These fake readings are fed through the physics engine, producing a fake confidence score and fake calibration state. There is no flag or condition that skips this when running against real hardware. The result is that the 82% confidence shown on the dashboard is computed from random numbers, not from your actual soil.

**FILES TO CHANGE**

* js/app.js — App.init() function and all MockAPI call sites

* index.html — add HARDWARE\_MODE flag declaration

**ADD to index.html — before the app.js script tag:**

\<script\>

  // Set to true when connected to real Hub hardware

  // Set via URL param: ?hardware=true

  const params \= new URLSearchParams(window.location.search);

  window.HARDWARE\_MODE \= params.get('hardware') \=== 'true';

\</script\>

**MODIFY App.init() in app.js — wrap all mock initialization:**

init: async function() {

  if (window.HARDWARE\_MODE) {

    // Real hardware path — fetch from Hub API

    await this.initFromHardware();

  } else {

    // Dev/sim path — existing mock flow

    MockAPI.init();

    MockAPI.seed();

    this.startMockLoop();

  }

  this.renderUI();

}

**ADD new method to App in app.js:**

initFromHardware: async function() {

  try {

    const res \= await fetch('/api/current');

    const data \= await res.json();

    // Build zones from real API response

    this.state.zones \= this.buildZonesFromAPI(data);

    this.startHardwarePolling();

  } catch(e) {

    console.error('\[App\] Hardware API unreachable:', e);

    this.showError('Cannot reach Hub. Check WiFi connection.');

  }

}

startHardwarePolling: function() {

  setInterval(async () \=\> {

    const res  \= await fetch('/api/current');

    const data \= await res.json();

    this.state.zones \= this.buildZonesFromAPI(data);

    this.renderUI();

  }, 15 \* 60 \* 1000); // poll every 15 min

}

**VERIFY**

* Load dashboard normally — should behave exactly as before (mock mode)

* Load dashboard with ?hardware=true — MockAPI.seed() should NOT be called

* Open console — confirm no '\[DATA\] Seeding...' log appears in hardware mode

* With real Hub connected, data should appear from /api/current endpoint

## **Issue 4 — Legacy Stub Functions Returning Constants**

**ROOT CAUSE**

The adapter block in physics.js that is being removed in Issue 1 also attaches five stub functions to window.Physics. These stubs always return the same hardcoded value regardless of input: VPD always 0.5 kPa, ET0 always 4.5 mm/day, drying rate always 0.2, decideAction always ALL\_GOOD, timeToCritical always 12 hours. The real implementations exist inside the PhysicsEngine class but were never wired to these legacy helpers. Any dashboard panel or firmware call using these gets wrong data.

**FILES TO CHANGE**

* js/physics.js — removing stubs (done as part of Issue 1\)

* js/app.js — update all call sites to use the real PhysicsEngine methods

**LOCATE in app.js — search for any of these call patterns and replace:**

// REMOVE these patterns wherever they appear:

Physics.calculateVPD(...)

Physics.calculateET0(...)

Physics.calculateDryingRate(...)

Physics.decideAction(...)

Physics.calculateTimeToCritical(...)

**REPLACE WITH — the PhysicsEngine already has real methods. Use these instead:**

// VPD — already computed in processSensorReading output

// Access it from the sample object: sample.vpd\_kPa

// ET0 — access from sample: sample.et0\_mm\_day

// Drying rate — access from sample: sample.drying\_rate

// Urgency/decision — access from sample: sample.urgency

// Values: 'high', 'medium', 'low'

// Time to critical — compute from sample fields:

// timeToCritical \= (theta \- theta\_refill) / Math.abs(drying\_rate)

// Returns hours. If drying\_rate is 0 or positive, return Infinity

**ADD helper to app.js for time-to-critical calculation:**

function calcTimeToCritical(sample) {

  const dr \= sample.drying\_rate || 0;

  if (dr \>= 0\) return Infinity;

  const gap \= sample.theta \- (sample.theta\_refill || 0);

  return gap / Math.abs(dr); // hours

}

**VERIFY**

* Search entire app.js for calculateVPD, calculateET0, calculateDryingRate, decideAction, calculateTimeToCritical — should return zero results

* Run a simulation cycle and confirm VPD and ET0 values in the dev console are non-constant and vary with temperature input

# **P1 — High/Medium: Fix Before Beta Farmer Handoff**

## **Issue 5 — History Cap Mismatch 200 vs 2880**

**ROOT CAUSE**

In app.js, MockAPI stores zone history and caps it at 200 entries with: if (this.zones\[zoneId\].history.length \> 200\) this.zones\[zoneId\].history.shift(). At 15-minute intervals, 200 entries equals roughly 50 hours of data. The PhysicsEngine is designed to work with up to 2880 entries (30 days). The Auto-Calibration state machine needs enough wetting and drying cycles — typically 7-14 days minimum — to reach NORMAL\_OPERATION. Capping zone history at 200 means the calibration data the physics engine would use is being discarded long before enough events are captured.

**FILES TO CHANGE**

* js/app.js — MockAPI zone history cap and main db cap

**LOCATE in app.js — find these two lines and change the cap values:**

// CHANGE this line in MockAPI.inject():

if (this.zones\[zoneId\].history.length \> 200\) this.zones\[zoneId\].history.shift();

// TO:

if (this.zones\[zoneId\].history.length \> 2880\) this.zones\[zoneId\].history.shift();

// CHANGE this line in MockAPI.inject() for the main db:

if (this.db.length \> 1000\) this.db.shift();

// TO:

if (this.db.length \> 2880\) this.db.shift();

**NOTE on firmware**

On the ESP32 firmware side, zone history is not stored in RAM — it is written to /logs/readings.csv on SD card as a rotating file. The SD card handles the 30-day buffer natively. No firmware change needed for this issue. The mismatch is purely a browser-side simulation artifact.

**VERIFY**

* Run a long simulation (use the existing 10-minute simulation button)

* After simulation, call MockAPI.zones\['A1'\].history.length in console

* Value should be able to grow beyond 200 without being trimmed prematurely

## **Issue 6 — No Mock vs Real Hardware Toggle**

**ROOT CAUSE**

MockAPI is used directly throughout app.js with no abstraction layer between it and the business logic. When the time comes to connect to real Hub hardware, there is no clean path — every call to MockAPI.inject(), MockAPI.getAllZones(), MockAPI.getLatest() etc would need to be individually found and replaced. The fix from Issue 3 begins this separation. This issue completes it.

**FILES TO CHANGE**

* js/app.js — create a DataSource abstraction that both MockAPI and RealAPI implement

**ADD to app.js — a RealAPI object with the same interface as MockAPI:**

const RealAPI \= {

  async getLatest(zoneId \= null) {

    const res \= await fetch('/api/current');

    const data \= await res.json();

    if (zoneId) return data.zones?.\[zoneId\]?.latest || null;

    return data;

  },

  async getAllZones() {

    const res \= await fetch('/api/current');

    return (await res.json()).zones || {};

  },

  async getSeries(zoneId) {

    const res \= await fetch('/api/zone/' \+ zoneId);

    return (await res.json()).history || \[\];

  },

  async getDiagnostics() {

    const res \= await fetch('/api/diagnostics');

    return res.json();

  }

};

**ADD DataSource selector — place this after both API objects:**

const DataSource \= window.HARDWARE\_MODE ? RealAPI : MockAPI;

**UPDATE all business logic in app.js — replace direct MockAPI calls:**

// REPLACE: MockAPI.getAllZones()   WITH: await DataSource.getAllZones()

// REPLACE: MockAPI.getLatest()    WITH: await DataSource.getLatest()

// REPLACE: MockAPI.getSeries(id)  WITH: await DataSource.getSeries(id)

**VERIFY**

* In mock mode: DataSource.getAllZones() should return mock zone data

* In hardware mode (?hardware=true): DataSource.getAllZones() should hit /api/current

* No direct MockAPI calls should remain outside of MockAPI's own internal methods

## **Issue 7 — No CropBand Pairing System**

**ROOT CAUSE**

The ESP-NOW receive callback in firmware/main.ino is not implemented. The Hub has no code to receive packets from CropBands, register new devices, or route data to per-device physics instances. This is entirely missing code, not broken code.

**FILES TO CHANGE**

* firmware/main.ino — add ESP-NOW initialization, receive callback, and device registry

**ADD to firmware/main.ino — the complete ESP-NOW implementation:**

// Add to top of file with other includes:

\#include \<esp\_now.h\>

// Packet structure — must match CropBand firmware exactly

typedef struct CropBandPacket {

  uint8\_t  mac\[6\];       // sender MAC

  uint16\_t raw\_adc;      // moisture ADC 0-4095

  int16\_t  temp\_x100;    // temp Celsius \* 100

  uint8\_t  battery\_pct;  // 0-100

  uint8\_t  crc8;         // checksum

} CropBandPacket;

// CRC8 validation

uint8\_t calcCRC8(uint8\_t\* data, size\_t len) {

  uint8\_t crc \= 0;

  for (size\_t i \= 0; i \< len; i++) {

    crc ^= data\[i\];

    for (int b \= 0; b \< 8; b++)

      crc \= crc & 0x80 ? (crc \<\< 1\) ^ 0x07 : crc \<\< 1;

  }

  return crc;

}

// Receive callback — runs on ESP-NOW packet arrival

void onEspNowReceive(const uint8\_t\* mac, const uint8\_t\* data, int len) {

  if (len \!= sizeof(CropBandPacket)) return;

  CropBandPacket pkt;

  memcpy(\&pkt, data, len);

  // Validate CRC

  uint8\_t expected \= calcCRC8((uint8\_t\*)\&pkt, len \- 1);

  if (pkt.crc8 \!= expected) {

    Serial.println('\[ESPNOW\] CRC fail — discarding packet');

    return;

  }

  // Build device ID string from MAC

  char macStr\[18\];

  snprintf(macStr,18,'%02X:%02X:%02X:%02X:%02X:%02X',

    pkt.mac\[0\],pkt.mac\[1\],pkt.mac\[2\],

    pkt.mac\[3\],pkt.mac\[4\],pkt.mac\[5\]);

  String deviceId \= String(macStr);

  // Look up in paired devices list

  if (\!isPairedDevice(deviceId)) {

    registerUnknownDevice(deviceId);

    return; // Do not process until farmer pairs it

  }

  // Route to per-device physics instance in Duktape

  float temp \= pkt.temp\_x100 / 100.0f;

  runPhysicsForDevice(pkt.raw\_adc, temp, time(nullptr), deviceId);

  saveCalibration(deviceId);

}

// Call this in setup() after WiFi.softAP():

void initEspNow() {

  esp\_now\_init();

  esp\_now\_register\_recv\_cb(onEspNowReceive);

  Serial.println('\[ESPNOW\] Receiver initialized');

}

**ADD device registry helpers to firmware/main.ino:**

bool isPairedDevice(const String& mac) {

  File f \= SD.open('/config/paired\_devices.json', FILE\_READ);

  if (\!f) return false;

  DynamicJsonDocument doc(4096);

  deserializeJson(doc, f); f.close();

  for (JsonObject d : doc\['devices'\].as\<JsonArray\>())

    if (String(d\['mac'\]|'') \== mac && (bool)(d\['paired'\]|false)) return true;

  return false;

}

void registerUnknownDevice(const String& mac) {

  Serial.println('\[ESPNOW\] New device seen: ' \+ mac);

  // Append to unpaired list — dashboard will show pairing prompt

  // Implementation: read paired\_devices.json, add entry with paired:false, write back

}

**VERIFY**

* Flash firmware with ESP-NOW enabled

* Power on a CropBand within range

* Serial monitor should show \[ESPNOW\] Receiver initialized on boot

* When CropBand transmits, should see \[ESPNOW\] New device seen: XX:XX:XX:XX:XX:XX

* After pairing in dashboard, next packet should be processed and written to SD card

## **Issue 8 — Hardcoded Zone Map ZONE\_CONFIG**

**ROOT CAUSE**

In app.js, ZONE\_CONFIG is a hardcoded object defining 16 zones in a 4x4 grid with fixed IDs like A1, A2, etc. This is not connected to any real device registry. The zone map renders these fake zones on load. The fix requires zones to be built dynamically from the paired devices list on the Hub.

**FILES TO CHANGE**

* js/app.js — remove ZONE\_CONFIG, replace with dynamic zone builder

**LOCATE and REMOVE the ZONE\_CONFIG constant at the top of app.js entirely.**

**ADD this dynamic zone builder to app.js:**

async function buildZoneMapFromDevices() {

  let devices;

  if (window.HARDWARE\_MODE) {

    const res \= await fetch('/api/devices');

    devices \= (await res.json()).devices || \[\];

  } else {

    // In mock mode return empty — no fake zones

    devices \= \[\];

  }

  const paired \= devices.filter(d \=\> d.paired);

  if (paired.length \=== 0\) {

    return { empty: true, message: 'No CropBands paired yet.' };

  }

  return paired.map((d, i) \=\> ({

    id:       d.mac,

    name:     d.zone\_name || 'Zone ' \+ (i \+ 1),

    row:      Math.floor(i / 4),

    col:      i % 4,

    active:   true,

    battery:  d.battery\_pct,

    lastSeen: d.last\_seen

  }));

}

**UPDATE the field map render function in app.js to call buildZoneMapFromDevices() instead of reading ZONE\_CONFIG.**

**UPDATE empty state — when no devices are paired, the field map should render:**

// Render this instead of an empty grid when buildZoneMapFromDevices returns empty:true

// '\<div class="empty-zone-map"\>No CropBands paired yet.

//  Connect a CropBand to begin zone monitoring.\</div\>'

**VERIFY**

* Load dashboard in mock mode — field map should show empty state with the no-bands message

* Load dashboard in hardware mode with no paired devices — same empty state

* Pair a CropBand via POST /api/devices/pair — zone should appear in map dynamically

## **Issue 9 — Onboarding Not Writing Correct Fields**

**ROOT CAUSE**

The physics engine reads crop and soil config from EXTERNAL\_CONFIG via configureCropSoil(). This is populated in firmware from user\_prefs.json. The onboarding form must write crop, soil, planting\_ts, and optionally flow\_rate\_lpm to localStorage (mock mode) and to /config/user\_prefs.json (hardware mode). Audit shows the onboarding form may not be collecting or writing planting\_ts as a Unix timestamp, and crop/soil keys may not match the expected string values in crop\_thresholds.json.

**FILES TO CHANGE**

* onboarding.html — field names, validation, and write logic

**REQUIRED FIELDS — onboarding must collect and write all of these:**

{

  onboarding\_complete: true,

  farmer\_name: string,

  crop: 'maize' | 'wheat' | 'tomato' | 'potato',

  soil: 'sandy\_loam' | 'loam' | 'clay\_loam' | 'clay',

  planting\_ts: number,  // Unix timestamp — Date.parse(input) / 1000

  flow\_rate\_lpm: number | null,  // Optional — null if farmer skips

  root\_depth\_cm: number,  // Default 30 if not asked

  setup\_date: number  // Unix timestamp of onboarding completion

}

**ADD validation to onboarding form submit handler:**

function validateOnboarding(data) {

  const validCrops \= \['maize','wheat','tomato','potato'\];

  const validSoils \= \['sandy\_loam','loam','clay\_loam','clay'\];

  if (\!validCrops.includes(data.crop)) return 'Invalid crop type';

  if (\!validSoils.includes(data.soil)) return 'Invalid soil type';

  if (\!data.planting\_ts || data.planting\_ts \> Date.now()/1000)

    return 'Planting date must be in the past';

  return null; // valid

}

**ADD warning banner to dashboard — show when physics engine uses defaults:**

// In App.init(), after loading prefs, check:

if (\!prefs.onboarding\_complete) {

  this.showBanner('Setup incomplete — using default crop settings.'),

  this.showBanner('Tap Settings to complete setup for accurate results.');

}

**VERIFY**

* Complete onboarding and inspect localStorage key agriscan\_user\_prefs

* Confirm planting\_ts is a number not a date string

* Confirm crop value is exactly one of the four valid strings

* Load dashboard — warning banner should NOT appear when onboarding is complete

## **Issue 10 — Flow Rate Required Not Optional**

**ROOT CAUSE**

Flow rate is used to calculate irrigation duration: duration\_minutes \= Dr\_mm / (flow\_rate\_lpm \* conversion\_factor). If the farmer does not know their flow rate, the current code either blocks completion or produces a division-by-zero result. Flow rate should be optional — if unknown, the system should show a duration range instead of a precise value.

**FILES TO CHANGE**

* onboarding.html — make flow rate field optional

* js/app.js — update irrigation duration calculation to handle null flow rate

**UPDATE onboarding.html — mark flow rate field as optional:**

\<label\>Flow Rate (L/min) \<span class='optional'\>(optional)\</span\>\</label\>

\<input type='number' id='flow\_rate' placeholder='Leave blank if unknown'\>

**UPDATE irrigation duration calculation in app.js:**

function calcIrrigationDuration(Dr\_mm, flowRate\_lpm) {

  if (\!Dr\_mm || Dr\_mm \<= 0\) return { text: 'No irrigation needed', precise: false };

  if (\!flowRate\_lpm) {

    // No flow rate — return a time range based on typical 5-15 L/min

    const minMins \= Math.round((Dr\_mm / 15\) \* 10);

    const maxMins \= Math.round((Dr\_mm / 5\)  \* 10);

    return { text: minMins \+ ' to ' \+ maxMins \+ ' minutes', precise: false };

  }

  const mins \= Math.round((Dr\_mm / flowRate\_lpm) \* 10);

  return { text: mins \+ ' minutes', precise: true };

}

**VERIFY**

* Complete onboarding without entering flow rate

* Dashboard should show irrigation duration as a range e.g. '12 to 36 minutes'

* Complete onboarding with flow rate 10 L/min

* Dashboard should show precise duration e.g. '18 minutes'

## **Issue 11 — No ADC Sensor Health Validation**

**ROOT CAUSE**

The firmware reads raw\_adc from the capacitive sensor and passes it directly to runPhysics() with no range check. A disconnected sensor floating will produce ADC values near 0 or near 4095\. A stuck sensor will produce the same value for hours. Neither condition is flagged. These bad values then corrupt the physics engine calibration.

**FILES TO CHANGE**

* firmware/main.ino — add validation before calling runPhysics()

**ADD to firmware/main.ino — sensor validation function:**

bool validateSensorReading(int raw\_adc, float temp\_c) {

  if (raw\_adc \< 200 || raw\_adc \> 3900\) {

    Serial.printf('\[QC\] ADC out of range: %d\\n', raw\_adc);

    logSensorError('ADC\_OUT\_OF\_RANGE', raw\_adc);

    return false;

  }

  if (temp\_c \< \-10.0f || temp\_c \> 60.0f) {

    Serial.printf('\[QC\] Temp out of range: %.1f\\n', temp\_c);

    logSensorError('TEMP\_OUT\_OF\_RANGE', temp\_c);

    return false;

  }

  return true;

}

void logSensorError(const char\* flag, float value) {

  File f \= SD.open('/logs/sensor\_errors.csv', FILE\_APPEND);

  if (f) {

    f.printf('%ld,%s,%.2f\\n', time(nullptr), flag, value);

    f.close();

  }

}

**UPDATE the main sensor read loop in firmware/main.ino:**

// BEFORE calling runPhysics(), wrap with validation:

if (validateSensorReading(raw\_adc, temp\_c)) {

  SampleData s \= runPhysics(raw\_adc, temp\_c, time(nullptr));

  dbManager.insertSample(s);

  saveCalibration('HUB\_ONBOARD');

} else {

  // Bad reading — skip physics, do not corrupt calibration

  Serial.println('\[QC\] Reading skipped due to sensor error');

}

**VERIFY**

* Disconnect the moisture sensor from the ESP32

* Serial monitor should show \[QC\] ADC out of range on the next read cycle

* SD card /logs/sensor\_errors.csv should contain the flagged reading

* Reconnect sensor — normal readings should resume without needing reboot

## **Issue 12 — Settings Saved to Browser localStorage**

**ROOT CAUSE**

app.js reads and writes user preferences to localStorage key agriscan\_user\_prefs. This means if you clear your browser, switch devices, or use a different phone to access the Hub WiFi, all settings are gone. The Hub SD card at /config/user\_prefs.json is the correct and already-existing source of truth. The browser should treat its copy as a read-through cache.

**FILES TO CHANGE**

* js/app.js — update all localStorage read/write calls

**ADD to app.js — two API helper functions:**

async function loadPrefsFromHub() {

  if (\!window.HARDWARE\_MODE) {

    const raw \= localStorage.getItem('agriscan\_user\_prefs');

    return raw ? JSON.parse(raw) : null;

  }

  try {

    const res \= await fetch('/api/config');

    return res.ok ? res.json() : null;

  } catch { return null; }

}

async function savePrefsToHub(prefs) {

  if (\!window.HARDWARE\_MODE) {

    localStorage.setItem('agriscan\_user\_prefs', JSON.stringify(prefs));

    return;

  }

  await fetch('/api/config', {

    method: 'POST',

    headers: { 'Content-Type': 'application/json' },

    body: JSON.stringify(prefs)

  });

  // Also cache locally for offline resilience

  localStorage.setItem('agriscan\_user\_prefs', JSON.stringify(prefs));

}

**REPLACE all direct localStorage calls in app.js with these two functions.**

**VERIFY**

* Save settings in mock mode — localStorage should be updated

* Save settings in hardware mode — POST /api/config should be called

* Simulate browser clear by deleting localStorage — in hardware mode, reloading should re-fetch prefs from Hub

## **Issue 13 — No SD Card Storage Management UI**

**ROOT CAUSE**

There are no API endpoints on the Hub for storage info, log download, or log clear. There is no UI in the dashboard to access storage. As the SD card fills, the only indication is eventual write failures — there is no proactive warning.

**FILES TO CHANGE**

* firmware/main.ino — add three new API endpoints

* js/app.js — add Storage panel to dev dashboard

**ADD to firmware/main.ino — three new server.on() handlers in setup():**

server.on('/api/storage', HTTP\_GET, \[\](AsyncWebServerRequest\* req) {

  uint64\_t total \= SD.totalBytes();

  uint64\_t used  \= SD.usedBytes();

  String json \= '{';

  json \+= '"total\_mb":' \+ String(total/1048576) \+ ',';

  json \+= '"used\_mb":' \+ String(used/1048576) \+ ',';

  json \+= '"free\_mb":' \+ String((total-used)/1048576);

  json \+= '}';

  req-\>send(200, 'application/json', json);

});

server.on('/api/logs/download', HTTP\_GET, \[\](AsyncWebServerRequest\* req) {

  req-\>send(SD, '/logs/readings.csv', 'text/csv',

    true); // true \= as attachment download

});

server.on('/api/logs/clear', HTTP\_DELETE, \[\](AsyncWebServerRequest\* req) {

  SD.remove('/logs/readings.csv');

  File f \= SD.open('/logs/readings.csv', FILE\_WRITE);

  if (f) {

    f.println('timestamp,zone,theta,urgency,confidence');

    f.close();

  }

  req-\>send(200, 'application/json', '{"cleared":true}');

});

**ADD storage panel to app.js dev dashboard section:**

async renderStoragePanel() {

  if (\!window.HARDWARE\_MODE) return;

  const res  \= await fetch('/api/storage');

  const data \= await res.json();

  const pct  \= Math.round((data.used\_mb / data.total\_mb) \* 100);

  // Render: progress bar, free/used MB labels,

  // Download Logs button \-\> GET /api/logs/download

  // Clear Logs button   \-\> DELETE /api/logs/clear (with confirm dialog)

  // Warn if pct \> 80 with yellow banner

  // Warn if pct \> 95 with red banner

}

**VERIFY**

* GET /api/storage returns valid JSON with total\_mb, used\_mb, free\_mb

* GET /api/logs/download triggers a file download in the browser

* DELETE /api/logs/clear wipes the log and returns cleared:true

* Storage panel shows correctly in dev dashboard with warning banners at 80% and 95% thresholds

## **Issue 14 — Desktop Dev Dashboard Not Built**

**ROOT CAUSE**

The desktop view is currently identical to the mobile view — just wider. There is no dedicated developer telemetry interface. This is entirely missing functionality.

**FILES TO CHANGE**

* js/app.js — add dev dashboard render logic

* index.html — add dev dashboard layout section

**REQUIRED PANELS for the desktop dev dashboard:**

1. Live Sensor Graph — Chart.js line chart, scrolling 24hr window, moisture % on Y axis, time on X axis, three horizontal threshold lines: theta\_fc (blue dashed), theta\_refill (orange dashed), theta\_pwp (red dashed). Lines update in real time as calibration learns.

2. Calibration State Panel — shows current state machine stage as a progress indicator (INIT / BASELINE / WETTING / DRAINAGE / FC\_ESTIMATE / NORMAL), confidence % as a progress bar, n\_events captured, n\_fc\_updates count.

3. Physics Event Log — scrolling list of timestamped events pulled from PhysicsEventLogger. Entries: STATE\_CHANGE, WETTING\_DETECTED, FC\_PLATEAU, FC\_UPDATE, SENSOR\_ERROR. Auto-scrolls to latest. Clearable.

4. Per-Device Panel — lists all registered CropBands by MAC, shows zone name, last seen timestamp, battery %, RSSI, and calibration confidence per device.

5. Storage Panel — from Issue 13\. Free space bar, download and clear buttons.

6. Preview Farmer View button — toggles between dev layout and the mobile farmer layout without changing URL.

**IMPLEMENT using existing Ctrl+Shift+D dev mode toggle in app.js. Extend toggleDevMode():**

toggleDevMode: function() {

  this.state.devMode \= \!this.state.devMode;

  if (this.state.devMode) {

    document.body.classList.add('dev-mode');

    this.renderDevDashboard(); // new method

  } else {

    document.body.classList.remove('dev-mode');

    this.renderFarmerDashboard(); // existing method

  }

}

**VERIFY**

* Press Ctrl+Shift+D — dev dashboard should appear with all 6 panels

* Live graph should update with each new sensor reading

* Threshold lines should move when calibration state updates

* Physics Event Log should show new entries in real time

* Press Ctrl+Shift+D again — farmer view should return

# **P2 — Quality: Implement After P1 Is Complete**

## **Issue 15 — Double Script Loading**

**ROOT CAUSE**

In index.html, physics.js and i18n.js are loaded as script tags. If app.js also imports or evals them, or if the Duktape firmware context re-evaluates them on reset, these files run twice. Double initialization of the PhysicsEngine global causes race conditions.

**FIX**

* Search index.html for all script tags. Count how many times physics.js appears. It must appear exactly once, after app.js.

* Search index.html for i18n.js — must appear exactly once, before app.js.

* In firmware/main.ino, confirm setupJS() is only called once in setup() and never inside the main loop.

* Add a guard at the top of physics.js to prevent double execution:

  if (typeof window \!== 'undefined' && window.\_\_agriscanPhysicsLoaded) {

    console.warn('\[Physics\] Already loaded — skipping re-init');

  } else {

    if (typeof window \!== 'undefined') window.\_\_agriscanPhysicsLoaded \= true;

    // ... rest of physics.js ...

  }

## **Issue 16 — Hardcoded Zone Label Names**

**ROOT CAUSE**

ZONE\_LABELS in app.js maps zone IDs to hardcoded strings like North Orchard and Central Bed. These are fictional. This entire constant is removed as part of Issue 8 when ZONE\_CONFIG is replaced. Zone names come from the paired\_devices record on SD card where the farmer assigns them during pairing.

**FIX**

* Confirm ZONE\_LABELS constant is fully removed as part of Issue 8\.

* Confirm all zone name rendering in app.js reads from zone.name property which comes from paired devices API response.

* Default zone name if farmer did not set one: 'Zone ' \+ index.

## **Issue 17 — Kalman Filter Not Implemented**

**ROOT CAUSE**

Raw ADC values go directly into the physics engine. Electrical noise causes small random fluctuations that appear as false drying or wetting events, degrading calibration accuracy.

**IMPLEMENT — add KalmanFilter class to physics.js:**

class KalmanFilter1D {

  constructor(Q=0.001, R=0.1) {

    this.Q \= Q; // process noise

    this.R \= R; // measurement noise

    this.x \= null; // state estimate

    this.P \= 1.0;  // error covariance

  }

  update(measurement) {

    if (this.x \=== null) { this.x \= measurement; return this.x; }

    this.P \+= this.Q;

    const K \= this.P / (this.P \+ this.R);

    this.x \+= K \* (measurement \- this.x);

    this.P \*= (1 \- K);

    return this.x;

  }

}

class KalmanFilter2D {

  constructor(Q=0.001, R=0.1) {

    this.Q \= Q; this.R \= R;

    this.x  \= \[null, 0\]; // \[value, rate\]

    this.P  \= \[\[1,0\],\[0,1\]\]; // covariance matrix

    this.dt \= 900; // 15 min in seconds

  }

  update(measurement) {

    if (this.x\[0\] \=== null) { this.x\[0\] \= measurement; return this.x; }

    // Predict

    const xp \= \[this.x\[0\] \+ this.x\[1\]\*this.dt, this.x\[1\]\];

    // Update

    const K \= this.P\[0\]\[0\] / (this.P\[0\]\[0\] \+ this.R);

    this.x\[0\] \= xp\[0\] \+ K\*(measurement \- xp\[0\]);

    this.x\[1\] \= xp\[1\];

    this.P\[0\]\[0\] \*= (1-K);

    return this.x; // \[smoothed\_theta, drying\_rate\_per\_second\]

  }

}

**INTEGRATE into PhysicsEngine constructor — add per-instance Kalman filters:**

constructor(soilParams \= DEFAULT\_SOIL) {

  // existing code...

  this.kalmMoisture \= new KalmanFilter2D(0.001, 0.05);

  this.kalmTemp     \= new KalmanFilter1D(0.001, 0.5);

}

**UPDATE processSensorReading() — apply Kalman before physics:**

processSensorReading(raw\_adc, temp\_c, timestamp) {

  const \[smoothTheta, dryingRate\] \= this.kalmMoisture.update(

    this.calibration.toVWC(raw\_adc, temp\_c)

  );

  const smoothTemp \= this.kalmTemp.update(temp\_c);

  // Pass smoothTheta and smoothTemp to physics instead of raw values

  // rest of method unchanged

}

**VERIFY**

* Run a simulation and inject a deliberate spike (ADC 900 while readings are \~600)

* The spike should produce minimal change in theta output compared to no Kalman

* Drying rate should be smooth, not jumping wildly between readings

AgriScan  |  Unity Provisions  |  unityprovisions.org  |  Confidential — Claude Code Implementation Document