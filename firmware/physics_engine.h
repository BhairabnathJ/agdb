#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include <Arduino.h>
#include <math.h>

// =============================================================================
// CONFIGURATION & CONSTANTS
// =============================================================================

// History buffer size - 288 = 3 days at 15-min intervals
// Reduced from JS 2880 to fit ESP32 RAM
#define HISTORY_MAX 288
#define FC_HISTORY_MAX 16

struct PhysicsConfig {
    // Pre-processing & QC
    float spike_z_thresh   = 6.0f;
    float stuck_eps        = 0.001f;
    float theta_bounds_min = 0.0f;
    float theta_bounds_max = 0.50f;

    // Temperature compensation
    float T_ref  = 20.0f;
    float a_temp = 0.0f;

    // Event detection
    float    wet_jump_thresh        = 0.02f;
    uint32_t wet_window             = 2 * 3600;
    uint32_t min_event_separation   = 12 * 3600;
    uint32_t post_event_ignore      = 1 * 3600;

    // FC detection
    uint32_t slope_window    = 2 * 3600;
    float    s_min           = 0.0005f;
    uint32_t hold_hours      = 8;
    float    fc_update_lambda = 0.25f;

    // Refill threshold
    float    theta_dry_percentile = 0.05f;
    uint32_t theta_dry_window     = 30;    // days
    float    eta_refill           = 0.5f;
    float    refill_hysteresis    = 0.01f;

    // Dynamics model
    float kd_init   = 0.05f;
    float ku_init   = 0.0005f;
    float beta_init = 1.0f;
};

extern PhysicsConfig CONFIG;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

struct DataPoint {
    time_t  timestamp;
    int     raw;
    float   temp_c;
    float   theta;
    bool    qc_valid;
};

struct QCResult {
    bool  valid;
    char  flags[64];   // comma-separated flag string
};

struct AvailableWater {
    float TAW_mm;
    float AW_mm;
    float Dr_mm;
    float fractionDepleted;
    float theta_fc;
    float theta_pwp;
};

struct IrrigationStatus {
    char  state[16];          // FULL, OPTIMAL, MONITOR, REFILL, UNKNOWN
    char  recommendation[64];
    char  urgency[8];         // none, low, medium, high
};

struct DrainageQuality {
    char  quality[16];        // good, poor, excessive
    float kd;
    char  message[64];
    char  recommendation[64];
};

struct SensorReading {
    // Raw + calibrated
    time_t  timestamp;
    int     raw_adc;
    float   temp_c;
    float   theta;
    bool    qc_valid;

    // Soil state
    float   theta_fc;
    float   theta_refill;
    float   psi_kPa;
    float   Se;

    // Available water
    float   TAW_mm;
    float   AW_mm;
    float   Dr_mm;
    float   fractionDepleted;

    // Dynamics
    float   dryingRate_per_hr;
    char    regime[16];       // wetting, drainage, drydown, stable, unknown

    // Status
    char    status[16];
    char    recommendation[64];
    char    urgency[8];

    // Calibration
    float   confidence;
    char    calibration_state[24];
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

float physics_clamp(float val, float minVal, float maxVal);
float physics_lerp(float a, float b, float t);
float physics_median(float* arr, int len);
float physics_percentile(float* arr, int len, float p);
float physics_std(float* arr, int len);
float physics_ewma(float current, float newValue, float lambda);

// =============================================================================
// SENSOR CALIBRATION
// =============================================================================

struct CalPoint {
    int   raw;
    float theta;
};

class SensorCalibration {
public:
    SensorCalibration();

    float rawToVWC(int raw);
    float temperatureCorrection(float theta, float temp_c);
    float calibrate(int raw, float temp_c);
    QCResult qualityControl(float theta, float temp_c,
                            DataPoint* history, int historyLen);

private:
    static const int NUM_CAL_POINTS = 5;
    CalPoint curve[NUM_CAL_POINTS];

    float siteOffset = 0.0f;
    float siteGain   = 1.0f;
};

// =============================================================================
// SOIL HYDRAULIC MODEL
// =============================================================================

struct SoilParams {
    float theta_r  = 0.078f;   // residual water content
    float theta_s  = 0.43f;    // saturated water content
    float alpha    = 0.036f;   // van Genuchten alpha (cm⁻¹)
    float n        = 1.56f;    // van Genuchten n
    float Ks       = 25.0f;    // saturated hydraulic conductivity (cm/day)
    float m        = 0.0f;     // computed: 1 - 1/n
    float theta_fc = 0.0f;     // field capacity (auto-set)
    float theta_pwp = 0.0f;    // permanent wilting point (auto-set)
};

class SoilModel {
public:
    SoilParams params;

    SoilModel();
    explicit SoilModel(SoilParams p);

    float vanGenuchten_theta(float psi_cm);
    float vanGenuchten_psi(float theta);
    float effectiveSaturation(float theta);
    float hydraulicConductivity(float theta);
    AvailableWater availableWater(float theta, float rootDepth_cm = 30.0f);
};

// =============================================================================
// EVENT DETECTION
// =============================================================================

class EventDetection {
public:
    EventDetection();

    bool  detectWetting(DataPoint* history, int len, float* delta_theta_out);
    float calculateDryingRate(DataPoint* history, int len,
                              uint32_t windowSeconds = 0);
    bool  detectFCPlateau(DataPoint* history, int len,
                          float* theta_fc_candidate_out);
    void  classifyRegime(DataPoint* history, int len,
                         float theta_fc, char* regimeOut);

    time_t lastEventTime = 0;

private:
    float recentTheta[32];   // scratch buffer for median calculations
};

// =============================================================================
// DYNAMICS MODEL
// =============================================================================

struct DynamicsParams {
    float kd        = 0.05f;    // drainage rate hr⁻¹
    float ku        = 0.0005f;  // drydown coefficient
    float beta      = 1.0f;     // drydown nonlinearity
    float theta_min = 0.05f;    // minimum theta for drydown
};

class DynamicsModel {
public:
    DynamicsParams params;

    DynamicsModel();

    float drainageRate(float theta, float theta_fc);
    float drydownRate(float theta);
    bool  fitDrainageParameter(DataPoint* data, int len,
                               float theta_fc, float* kd_out);
    bool  fitDrydownParameters(DataPoint* data, int len,
                               float* ku_out, float* beta_out,
                               float* theta_min_out);
};

// =============================================================================
// AUTO-CALIBRATION
// =============================================================================

enum CalibrationState {
    CAL_INIT = 0,
    CAL_BASELINE_MONITORING,
    CAL_WETTING_EVENT,
    CAL_DRAINAGE_TRACKING,
    CAL_FC_ESTIMATE,
    CAL_DRYDOWN_FIT,
    CAL_NORMAL_OPERATION
};

struct CalibrationResult {
    CalibrationState state;
    float theta_fc_star;
    float theta_refill_star;
    float confidence;
    int   n_events;
    int   n_fc_updates;
    float qc_pass_rate;
};

class AutoCalibration {
public:
    AutoCalibration(SoilModel* soilModel,
                    EventDetection* eventDetection,
                    DynamicsModel* dynamicsModel);

    void             update(DataPoint& point, DataPoint* history, int historyLen);
    CalibrationResult getCalibrationState();

    float theta_fc_star     = -1.0f;   // -1 = not yet set
    float theta_refill_star = -1.0f;

private:
    SoilModel*      soilModel;
    EventDetection* eventDetection;
    DynamicsModel*  dynamicsModel;

    CalibrationState state = CAL_INIT;

    float    fc_history[FC_HISTORY_MAX];
    int      fc_history_len = 0;
    float    confidence     = 0.2f;
    float    fc_candidate   = -1.0f;
    time_t   currentEventStart = 0;

    struct {
        int   n_events      = 0;
        int   n_fc_updates  = 0;
        int   qc_pass_count = 0;
        int   qc_total_count= 0;
    } stats;

    void state_init(DataPoint* history, int len);
    void state_baseline(DataPoint& point, DataPoint* history, int len);
    void state_wetting(DataPoint& point);
    void state_drainage(DataPoint& point, DataPoint* history, int len);
    void state_fc_estimate(DataPoint* history, int len);
    void state_drydown_fit(DataPoint* history, int len);
    void state_normal(DataPoint& point, DataPoint* history, int len);
    void updateRefillThreshold(DataPoint* history, int len);
    void updateConfidence();

    const char* stateToString(CalibrationState s);
};

// =============================================================================
// MAIN PHYSICS ENGINE
// =============================================================================

struct ExternalConfig {
    char  crop[16]      = "";
    char  soil[16]      = "";
    long  planting_ts   = 0;
    float p             = -1.0f;
    float theta_fc      = -1.0f;
    float theta_wp      = -1.0f;
    float theta_refill  = -1.0f;
};

class PhysicsEngine {
public:
    PhysicsEngine();

    void          configureCropSoil(const char* crop, const char* soil,
                                    float p, float theta_fc, float theta_wp,
                                    float theta_refill, long planting_ts);

    SensorReading processSensorReading(int raw, float temp_c, time_t timestamp);

    // Getters for external use
    CalibrationResult getCalibrationState();
    int               historyLen() { return _historyLen; }
    DataPoint*        getHistory() { return _history; }

    DrainageQuality   assessDrainageQuality();
    IrrigationStatus  getIrrigationStatus(float theta, float theta_fc,
                                          float theta_refill, float dryingRate);

private:
    SensorCalibration calibration;
    SoilModel         soilModel;
    EventDetection    eventDetection;
    DynamicsModel     dynamicsModel;
    AutoCalibration   autoCalibration;
    ExternalConfig    extConfig;

    DataPoint _history[HISTORY_MAX];
    int       _historyLen = 0;

    void pushHistory(DataPoint& p);
    void calculateMetrics(DataPoint& point, SensorReading& out);
};

// Global singleton — accessed from main.ino
extern PhysicsEngine Physics;

#endif // PHYSICS_ENGINE_H
