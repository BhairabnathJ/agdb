#include "physics_engine.h"
#include <string.h>
#include <stdlib.h>

// =============================================================================
// GLOBAL CONFIG INSTANCE
// =============================================================================

PhysicsConfig CONFIG;

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

float physics_clamp(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

float physics_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static int _cmp_float(const void* a, const void* b) {
    float fa = *(float*)a, fb = *(float*)b;
    return (fa > fb) - (fa < fb);
}

float physics_median(float* arr, int len) {
    if (len <= 0) return 0.0f;
    float tmp[len];
    memcpy(tmp, arr, len * sizeof(float));
    qsort(tmp, len, sizeof(float), _cmp_float);
    int mid = len / 2;
    return (len % 2 == 0) ? (tmp[mid - 1] + tmp[mid]) * 0.5f : tmp[mid];
}

float physics_percentile(float* arr, int len, float p) {
    if (len <= 0) return 0.0f;
    float tmp[len];
    memcpy(tmp, arr, len * sizeof(float));
    qsort(tmp, len, sizeof(float), _cmp_float);
    float index = (p / 100.0f) * (len - 1);
    int   lower = (int)index;
    int   upper = lower + 1;
    if (upper >= len) return tmp[len - 1];
    float weight = index - lower;
    return tmp[lower] * (1.0f - weight) + tmp[upper] * weight;
}

float physics_std(float* arr, int len) {
    if (len <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < len; i++) sum += arr[i];
    float mean = sum / len;
    float var  = 0.0f;
    for (int i = 0; i < len; i++) var += (arr[i] - mean) * (arr[i] - mean);
    return sqrtf(var / len);
}

float physics_ewma(float current, float newValue, float lambda) {
    if (current < 0.0f) return newValue;   // uninitialized sentinel
    return (1.0f - lambda) * current + lambda * newValue;
}

// =============================================================================
// SENSOR CALIBRATION
// =============================================================================

SensorCalibration::SensorCalibration() {
    // Default factory calibration curve (capacitive sensor v1.2)
    curve[0] = {250,  0.00f};
    curve[1] = {450,  0.10f};
    curve[2] = {650,  0.25f};
    curve[3] = {850,  0.40f};
    curve[4] = {1000, 0.50f};
}

float SensorCalibration::rawToVWC(int raw) {
    if (raw <= curve[0].raw) return curve[0].theta;
    if (raw >= curve[NUM_CAL_POINTS - 1].raw) return curve[NUM_CAL_POINTS - 1].theta;

    for (int i = 1; i < NUM_CAL_POINTS; i++) {
        if (raw <= curve[i].raw) {
            float x0 = curve[i-1].raw,  y0 = curve[i-1].theta;
            float x1 = curve[i].raw,    y1 = curve[i].theta;
            float t  = (raw - x0) / (x1 - x0);
            return physics_lerp(y0, y1, t);
        }
    }
    return curve[NUM_CAL_POINTS - 1].theta;
}

float SensorCalibration::temperatureCorrection(float theta, float temp_c) {
    return theta + CONFIG.a_temp * (temp_c - CONFIG.T_ref);
}

float SensorCalibration::calibrate(int raw, float temp_c) {
    float theta = rawToVWC(raw);
    theta = siteGain * theta + siteOffset;
    theta = temperatureCorrection(theta, temp_c);
    theta = physics_clamp(theta, CONFIG.theta_bounds_min, CONFIG.theta_bounds_max);
    return theta;
}

QCResult SensorCalibration::qualityControl(float theta, float temp_c,
                                            DataPoint* history, int historyLen) {
    QCResult result;
    result.valid = true;
    result.flags[0] = '\0';

    auto addFlag = [&](const char* f) {
        if (result.flags[0] != '\0') strncat(result.flags, ",", 63);
        strncat(result.flags, f, 63);
        result.valid = false;
    };

    // Bounds check
    if (theta < CONFIG.theta_bounds_min || theta > CONFIG.theta_bounds_max)
        addFlag("OUT_OF_BOUNDS");

    // Spike detection (z-score)
    if (historyLen >= 3) {
        int  n      = min(historyLen, 5);
        float recent[5];
        for (int i = 0; i < n; i++)
            recent[i] = history[historyLen - n + i].theta;
        float sum = 0;
        for (int i = 0; i < n; i++) sum += recent[i];
        float mean = sum / n;
        float sd   = physics_std(recent, n);
        float z    = fabsf((theta - mean) / (sd + 0.001f));
        if (z > CONFIG.spike_z_thresh) addFlag("SPIKE_DETECTED");
    }

    // Stuck sensor check
    if (historyLen >= 10) {
        float recent[10];
        for (int i = 0; i < 10; i++)
            recent[i] = history[historyLen - 10 + i].theta;
        float mn = recent[0], mx = recent[0];
        for (int i = 1; i < 10; i++) {
            if (recent[i] < mn) mn = recent[i];
            if (recent[i] > mx) mx = recent[i];
        }
        if ((mx - mn) < CONFIG.stuck_eps) addFlag("SENSOR_STUCK");
    }

    // Temperature sanity
    if (temp_c < -10.0f || temp_c > 60.0f) addFlag("TEMP_OUT_OF_RANGE");

    return result;
}

// =============================================================================
// SOIL MODEL
// =============================================================================

SoilModel::SoilModel() {
    params.m = 1.0f - (1.0f / params.n);
    params.theta_fc  = vanGenuchten_theta(330.0f);    // -33 kPa
    params.theta_pwp = vanGenuchten_theta(15000.0f);  // -1500 kPa
}

SoilModel::SoilModel(SoilParams p) : params(p) {
    params.m = 1.0f - (1.0f / params.n);
    if (params.theta_fc  <= 0.0f) params.theta_fc  = vanGenuchten_theta(330.0f);
    if (params.theta_pwp <= 0.0f) params.theta_pwp = vanGenuchten_theta(15000.0f);
}

float SoilModel::vanGenuchten_theta(float psi_cm) {
    const float theta_r = params.theta_r;
    const float theta_s = params.theta_s;
    const float alpha   = params.alpha;
    const float n       = params.n;
    const float m       = params.m;

    if (psi_cm <= 0.0f) return theta_s;

    float term  = 1.0f + powf(alpha * psi_cm, n);
    return theta_r + (theta_s - theta_r) / powf(term, m);
}

float SoilModel::vanGenuchten_psi(float theta) {
    const float theta_r = params.theta_r;
    const float theta_s = params.theta_s;
    const float alpha   = params.alpha;
    const float n       = params.n;
    const float m       = params.m;

    theta = physics_clamp(theta, theta_r + 0.001f, theta_s - 0.001f);

    float Se   = (theta - theta_r) / (theta_s - theta_r);
    float term = powf(1.0f / Se, 1.0f / m) - 1.0f;
    return powf(term, 1.0f / n) / alpha;   // cm H2O
}

float SoilModel::effectiveSaturation(float theta) {
    return physics_clamp(
        (theta - params.theta_r) / (params.theta_s - params.theta_r),
        0.0f, 1.0f);
}

float SoilModel::hydraulicConductivity(float theta) {
    float Se = effectiveSaturation(theta);
    float m  = params.m;
    const float L = 0.5f;

    if (Se >= 1.0f) return params.Ks;
    if (Se <= 0.01f) return params.Ks * 1e-10f;

    float inner = 1.0f - powf(1.0f - powf(Se, 1.0f / m), m);
    return params.Ks * powf(Se, L) * inner * inner;
}

AvailableWater SoilModel::availableWater(float theta, float rootDepth_cm) {
    float theta_fc  = params.theta_fc;
    float theta_pwp = params.theta_pwp;

    float TAW = (theta_fc - theta_pwp) * rootDepth_cm * 10.0f;
    float AW  = fmaxf(0.0f, (theta - theta_pwp) * rootDepth_cm * 10.0f);
    float Dr  = TAW - AW;
    float fd  = (TAW > 0.0f) ? physics_clamp(Dr / TAW, 0.0f, 1.0f) : 0.0f;

    return {TAW, AW, Dr, fd, theta_fc, theta_pwp};
}

// =============================================================================
// EVENT DETECTION
// =============================================================================

EventDetection::EventDetection() {}

bool EventDetection::detectWetting(DataPoint* history, int len,
                                    float* delta_theta_out) {
    *delta_theta_out = 0.0f;
    if (len < 2) return false;

    time_t now         = history[len - 1].timestamp;
    time_t windowStart = now - CONFIG.wet_window;

    // Find window start index
    int startIdx = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (history[i].timestamp < windowStart) { startIdx = i + 1; break; }
    }

    if ((len - startIdx) < 2) return false;

    float thetaStart = history[startIdx].theta;
    float thetaEnd   = history[len - 1].theta;
    float delta      = thetaEnd - thetaStart;
    *delta_theta_out = delta;

    if (delta >= CONFIG.wet_jump_thresh) {
        if (lastEventTime > 0 &&
            (now - lastEventTime) < (time_t)CONFIG.min_event_separation)
            return false;
        lastEventTime = now;
        return true;
    }
    return false;
}

float EventDetection::calculateDryingRate(DataPoint* history, int len,
                                           uint32_t windowSeconds) {
    if (len < 3) return 0.0f;
    if (windowSeconds == 0) windowSeconds = CONFIG.slope_window;

    time_t now         = history[len - 1].timestamp;
    time_t windowStart = now - windowSeconds;

    int startIdx = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (history[i].timestamp < windowStart) { startIdx = i + 1; break; }
    }

    int n = len - startIdx;
    if (n < 3) return 0.0f;

    // Linear regression
    time_t t0   = history[startIdx].timestamp;
    float sumX  = 0, sumY = 0, sumXY = 0, sumX2 = 0;

    for (int i = startIdx; i < len; i++) {
        float x = (history[i].timestamp - t0) / 3600.0f;  // hours
        float y = history[i].theta;
        sumX  += x;
        sumY  += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    float denom = n * sumX2 - sumX * sumX;
    if (fabsf(denom) < 1e-9f) return 0.0f;

    return (n * sumXY - sumX * sumY) / denom;  // m³/m³/hr
}

bool EventDetection::detectFCPlateau(DataPoint* history, int len,
                                      float* theta_fc_candidate_out) {
    *theta_fc_candidate_out = 0.0f;
    if (len < 20) return false;

    float rate = calculateDryingRate(history, len);

    if (fabsf(rate) >= CONFIG.s_min) return false;

    // Check sustained for hold_hours
    uint32_t holdSeconds = CONFIG.hold_hours * 3600;
    time_t   now         = history[len - 1].timestamp;
    time_t   plateauStart = now - holdSeconds;

    int plateauCount = 0;
    float thetaVals[HISTORY_MAX];

    for (int i = 0; i < len; i++) {
        if (history[i].timestamp >= plateauStart) {
            thetaVals[plateauCount++] = history[i].theta;
        }
    }

    if (plateauCount < 10) return false;

    *theta_fc_candidate_out = physics_median(thetaVals, plateauCount);
    return true;
}

void EventDetection::classifyRegime(DataPoint* history, int len,
                                     float theta_fc, char* regimeOut) {
    if (len < 5) { strcpy(regimeOut, "unknown"); return; }

    float rate         = calculateDryingRate(history, len);
    float currentTheta = history[len - 1].theta;

    if (rate > 0.001f)              { strcpy(regimeOut, "wetting");  return; }
    if (fabsf(rate) < CONFIG.s_min) { strcpy(regimeOut, "stable");   return; }

    if (theta_fc > 0.0f) {
        if (currentTheta > theta_fc) { strcpy(regimeOut, "drainage"); return; }
        else                         { strcpy(regimeOut, "drydown");  return; }
    }

    strcpy(regimeOut, rate < 0.0f ? "drydown" : "stable");
}

// =============================================================================
// DYNAMICS MODEL
// =============================================================================

DynamicsModel::DynamicsModel() {
    params.kd        = CONFIG.kd_init;
    params.ku        = CONFIG.ku_init;
    params.beta      = CONFIG.beta_init;
    params.theta_min = 0.05f;
}

float DynamicsModel::drainageRate(float theta, float theta_fc) {
    if (theta <= theta_fc) return 0.0f;
    return -params.kd * (theta - theta_fc);
}

float DynamicsModel::drydownRate(float theta) {
    if (theta <= params.theta_min) return 0.0f;
    return -params.ku * powf(theta - params.theta_min, params.beta);
}

bool DynamicsModel::fitDrainageParameter(DataPoint* data, int len,
                                          float theta_fc, float* kd_out) {
    if (len < 5) return false;

    time_t t0     = data[0].timestamp;
    float  theta0 = data[0].theta;

    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int   n    = 0;

    for (int i = 0; i < len; i++) {
        if (data[i].theta <= theta_fc) continue;
        float delta = data[i].theta - theta_fc;
        if (delta < 0.001f) continue;

        float x = (data[i].timestamp - t0) / 3600.0f;
        float y = logf(delta);
        sumX  += x;  sumY  += y;
        sumXY += x * y;  sumX2 += x * x;
        n++;
    }

    if (n < 5) return false;

    float denom = n * sumX2 - sumX * sumX;
    if (fabsf(denom) < 1e-9f) return false;

    float slope = (n * sumXY - sumX * sumY) / denom;
    float kd    = -slope;

    if (kd < 0.001f || kd > 1.0f) return false;

    *kd_out = kd;
    return true;
}

bool DynamicsModel::fitDrydownParameters(DataPoint* data, int len,
                                          float* ku_out, float* beta_out,
                                          float* theta_min_out) {
    if (len < 10) return false;

    float theta_min = data[0].theta;
    for (int i = 1; i < len; i++)
        if (data[i].theta < theta_min) theta_min = data[i].theta;
    theta_min -= 0.01f;

    float theta0  = data[0].theta;
    float theta_e = data[len - 1].theta;
    float t_end   = (data[len - 1].timestamp - data[0].timestamp) / 3600.0f;

    if (theta_e <= theta_min || theta0 <= theta_min || t_end <= 0.0f)
        return false;

    float ku = -logf((theta_e - theta_min) / (theta0 - theta_min)) / t_end;

    if (ku <= 0.0f || ku >= 0.1f) return false;

    *ku_out        = ku;
    *beta_out      = 1.0f;
    *theta_min_out = theta_min;
    return true;
}

// =============================================================================
// AUTO-CALIBRATION
// =============================================================================

AutoCalibration::AutoCalibration(SoilModel*      sm,
                                  EventDetection* ed,
                                  DynamicsModel*  dm)
    : soilModel(sm), eventDetection(ed), dynamicsModel(dm) {
    memset(fc_history, 0, sizeof(fc_history));
}

void AutoCalibration::update(DataPoint& point, DataPoint* history, int historyLen) {
    stats.qc_total_count++;
    if (point.qc_valid) stats.qc_pass_count++;

    switch (state) {
        case CAL_INIT:               state_init(history, historyLen);             break;
        case CAL_BASELINE_MONITORING: state_baseline(point, history, historyLen); break;
        case CAL_WETTING_EVENT:       state_wetting(point);                       break;
        case CAL_DRAINAGE_TRACKING:   state_drainage(point, history, historyLen); break;
        case CAL_FC_ESTIMATE:         state_fc_estimate(history, historyLen);     break;
        case CAL_DRYDOWN_FIT:         state_drydown_fit(history, historyLen);     break;
        case CAL_NORMAL_OPERATION:    state_normal(point, history, historyLen);   break;
    }

    updateConfidence();
}

void AutoCalibration::state_init(DataPoint* history, int len) {
    if (len < 10) return;   // Need at least 10 samples before starting

    // Seed FC from van Genuchten model
    if (theta_fc_star < 0.0f)
        theta_fc_star = soilModel->params.theta_fc;

    // Estimate theta_dry from history percentile
    float thetaVals[HISTORY_MAX];
    for (int i = 0; i < len; i++) thetaVals[i] = history[i].theta;
    float theta_dry = physics_percentile(thetaVals, len,
                                         CONFIG.theta_dry_percentile * 100.0f);

    theta_refill_star = theta_fc_star -
        CONFIG.eta_refill * (theta_fc_star - theta_dry);

    stats.n_fc_updates++;
    state = CAL_BASELINE_MONITORING;
}

void AutoCalibration::state_baseline(DataPoint& point, DataPoint* history, int len) {
    float delta = 0.0f;
    if (eventDetection->detectWetting(history, len, &delta)) {
        stats.n_events++;
        state              = CAL_WETTING_EVENT;
        currentEventStart  = point.timestamp;
    }
}

void AutoCalibration::state_wetting(DataPoint& point) {
    if ((point.timestamp - currentEventStart) > (time_t)CONFIG.post_event_ignore)
        state = CAL_DRAINAGE_TRACKING;
}

void AutoCalibration::state_drainage(DataPoint& point, DataPoint* history, int len) {
    float candidate = 0.0f;
    if (eventDetection->detectFCPlateau(history, len, &candidate)) {
        state        = CAL_FC_ESTIMATE;
        fc_candidate = candidate;
        return;
    }

    char regime[16];
    eventDetection->classifyRegime(history, len, theta_fc_star, regime);
    if (strcmp(regime, "drydown") == 0)
        state = CAL_NORMAL_OPERATION;
}

void AutoCalibration::state_fc_estimate(DataPoint* history, int len) {
    // EWMA update of FC
    if (theta_fc_star < 0.0f) {
        theta_fc_star = fc_candidate;
    } else {
        theta_fc_star = physics_ewma(theta_fc_star, fc_candidate,
                                     CONFIG.fc_update_lambda);
    }

    if (fc_history_len < FC_HISTORY_MAX)
        fc_history[fc_history_len++] = theta_fc_star;

    stats.n_fc_updates++;
    updateRefillThreshold(history, len);

    // Try to fit kd from drainage data
    float kd = 0.0f;
    if (dynamicsModel->fitDrainageParameter(history, len, theta_fc_star, &kd))
        dynamicsModel->params.kd = kd;

    state = CAL_DRYDOWN_FIT;
}

void AutoCalibration::state_drydown_fit(DataPoint* history, int len) {
    char regime[16];
    eventDetection->classifyRegime(history, len, theta_fc_star, regime);

    if (strcmp(regime, "drydown") == 0 && len >= 10) {
        float ku = 0, beta = 0, theta_min = 0;
        if (dynamicsModel->fitDrydownParameters(history, len, &ku, &beta, &theta_min)) {
            dynamicsModel->params.ku        = ku;
            dynamicsModel->params.beta      = beta;
            dynamicsModel->params.theta_min = theta_min;
        }
    }

    state = CAL_NORMAL_OPERATION;
}

void AutoCalibration::state_normal(DataPoint& point, DataPoint* history, int len) {
    float delta = 0.0f;
    if (eventDetection->detectWetting(history, len, &delta)) {
        stats.n_events++;
        state             = CAL_WETTING_EVENT;
        currentEventStart = point.timestamp;
    }
}

void AutoCalibration::updateRefillThreshold(DataPoint* history, int len) {
    if (len < 10) return;

    float thetaVals[HISTORY_MAX];
    for (int i = 0; i < len; i++) thetaVals[i] = history[i].theta;
    float theta_dry = physics_percentile(thetaVals, len,
                                         CONFIG.theta_dry_percentile * 100.0f);

    theta_refill_star = theta_fc_star -
        CONFIG.eta_refill * (theta_fc_star - theta_dry);
}

void AutoCalibration::updateConfidence() {
    // Component 1: events captured (saturates at 8)
    float eventScore = fminf((float)stats.n_events / 8.0f, 1.0f);

    // Component 2: FC stability
    float stabilityScore = 0.5f;
    if (fc_history_len >= 3) {
        float fc_sd = physics_std(fc_history, fc_history_len);
        stabilityScore = expf(-fc_sd / 0.02f);
    } else if (fc_history_len > 0) {
        stabilityScore = 0.6f + (fc_history_len / 3.0f) * 0.2f;
    }

    // Component 3: QC pass rate
    float qcScore = (stats.qc_total_count > 0)
        ? (float)stats.qc_pass_count / stats.qc_total_count
        : 0.5f;

    // Component 4: data volume
    float dataScore = fminf((float)stats.qc_total_count / 50.0f, 1.0f);

    // State progression bonus
    float stateBonus[] = {0.0f, 0.05f, 0.1f, 0.15f, 0.2f, 0.2f, 0.25f};
    float sb = stateBonus[(int)state];

    confidence = 0.40f * eventScore +
                 0.25f * stabilityScore +
                 0.20f * qcScore +
                 0.15f * dataScore +
                 sb;

    confidence = physics_clamp(confidence, 0.0f, 1.0f);
}

CalibrationResult AutoCalibration::getCalibrationState() {
    CalibrationResult r;
    r.state           = state;
    r.theta_fc_star   = theta_fc_star;
    r.theta_refill_star = theta_refill_star;
    r.confidence      = confidence;
    r.n_events        = stats.n_events;
    r.n_fc_updates    = stats.n_fc_updates;
    r.qc_pass_rate    = (stats.qc_total_count > 0)
        ? (float)stats.qc_pass_count / stats.qc_total_count
        : 0.0f;
    return r;
}

const char* AutoCalibration::stateToString(CalibrationState s) {
    switch (s) {
        case CAL_INIT:                 return "INIT";
        case CAL_BASELINE_MONITORING:  return "BASELINE_MONITORING";
        case CAL_WETTING_EVENT:        return "WETTING_EVENT";
        case CAL_DRAINAGE_TRACKING:    return "DRAINAGE_TRACKING";
        case CAL_FC_ESTIMATE:          return "FC_ESTIMATE";
        case CAL_DRYDOWN_FIT:          return "DRYDOWN_FIT";
        case CAL_NORMAL_OPERATION:     return "NORMAL_OPERATION";
        default:                       return "UNKNOWN";
    }
}

// =============================================================================
// PHYSICS ENGINE
// =============================================================================

PhysicsEngine Physics;   // Global singleton

PhysicsEngine::PhysicsEngine()
    : autoCalibration(&soilModel, &eventDetection, &dynamicsModel) {
    _historyLen = 0;
    memset(&extConfig, 0, sizeof(extConfig));
    extConfig.p            = -1.0f;
    extConfig.theta_fc     = -1.0f;
    extConfig.theta_wp     = -1.0f;
    extConfig.theta_refill = -1.0f;
}

void PhysicsEngine::configureCropSoil(const char* crop, const char* soil,
                                       float p, float theta_fc, float theta_wp,
                                       float theta_refill, long planting_ts) {
    strncpy(extConfig.crop, crop, 15);
    strncpy(extConfig.soil, soil, 15);
    extConfig.planting_ts = planting_ts;

    if (p > 0.0f) {
        extConfig.p       = p;
        CONFIG.eta_refill = p;
    }
    if (theta_fc > 0.0f) {
        extConfig.theta_fc             = theta_fc;
        soilModel.params.theta_fc      = theta_fc;
        autoCalibration.theta_fc_star  = theta_fc;
    }
    if (theta_wp > 0.0f) {
        extConfig.theta_wp              = theta_wp;
        soilModel.params.theta_pwp      = theta_wp;
    }
    if (theta_refill > 0.0f) {
        extConfig.theta_refill                = theta_refill;
        autoCalibration.theta_refill_star     = theta_refill;
    } else if (extConfig.theta_fc > 0.0f && extConfig.theta_wp > 0.0f && extConfig.p > 0.0f) {
        autoCalibration.theta_refill_star =
            extConfig.theta_fc - extConfig.p * (extConfig.theta_fc - extConfig.theta_wp);
    }

    Serial.printf("[Physics] Configured: crop=%s soil=%s fc=%.3f refill=%.3f\n",
                  crop, soil, autoCalibration.theta_fc_star,
                  autoCalibration.theta_refill_star);
}

void PhysicsEngine::pushHistory(DataPoint& p) {
    if (_historyLen < HISTORY_MAX) {
        _history[_historyLen++] = p;
    } else {
        // Shift left (drop oldest)
        memmove(_history, _history + 1, (HISTORY_MAX - 1) * sizeof(DataPoint));
        _history[HISTORY_MAX - 1] = p;
    }
}

SensorReading PhysicsEngine::processSensorReading(int raw, float temp_c,
                                                   time_t timestamp) {
    // Step 1: Calibrate
    float theta = calibration.calibrate(raw, temp_c);

    // Step 2: QC
    QCResult qc = calibration.qualityControl(theta, temp_c,
                                              _history, _historyLen);

    // Step 3: Build data point
    DataPoint point;
    point.timestamp = timestamp;
    point.raw       = raw;
    point.temp_c    = temp_c;
    point.theta     = theta;
    point.qc_valid  = qc.valid;

    // Step 4: Add to history
    pushHistory(point);

    // Step 5: Update auto-calibration
    if (qc.valid)
        autoCalibration.update(point, _history, _historyLen);

    // Step 6: Build output
    SensorReading out;
    out.timestamp = timestamp;
    out.raw_adc   = raw;
    out.temp_c    = temp_c;
    out.theta     = theta;
    out.qc_valid  = qc.valid;

    calculateMetrics(point, out);
    return out;
}

void PhysicsEngine::calculateMetrics(DataPoint& point, SensorReading& out) {
    CalibrationResult cal = autoCalibration.getCalibrationState();

    float theta_fc     = (cal.theta_fc_star > 0.0f)
                         ? cal.theta_fc_star
                         : soilModel.params.theta_fc;
    float theta_refill = cal.theta_refill_star;

    out.theta_fc      = theta_fc;
    out.theta_refill  = theta_refill;

    // Matric potential
    float psi_cm = soilModel.vanGenuchten_psi(point.theta);
    out.psi_kPa  = psi_cm / 10.0f;

    // Effective saturation
    out.Se = soilModel.effectiveSaturation(point.theta);

    // Available water
    AvailableWater aw = soilModel.availableWater(point.theta);
    out.TAW_mm          = aw.TAW_mm;
    out.AW_mm           = aw.AW_mm;
    out.Dr_mm           = aw.Dr_mm;
    out.fractionDepleted = aw.fractionDepleted;

    // Drying rate
    out.dryingRate_per_hr = eventDetection.calculateDryingRate(
        _history, _historyLen);

    // Regime
    eventDetection.classifyRegime(_history, _historyLen,
                                  theta_fc, out.regime);

    // Irrigation status
    IrrigationStatus status = getIrrigationStatus(
        point.theta, theta_fc, theta_refill, out.dryingRate_per_hr);
    strncpy(out.status,         status.state,          15);
    strncpy(out.recommendation, status.recommendation, 63);
    strncpy(out.urgency,        status.urgency,         7);

    // Calibration
    out.confidence = cal.confidence;
    snprintf(out.calibration_state, 24, "%s", 
             cal.state == CAL_INIT                ? "INIT" :
             cal.state == CAL_BASELINE_MONITORING ? "BASELINE" :
             cal.state == CAL_WETTING_EVENT       ? "WETTING" :
             cal.state == CAL_DRAINAGE_TRACKING   ? "DRAINAGE" :
             cal.state == CAL_FC_ESTIMATE         ? "FC_ESTIMATE" :
             cal.state == CAL_DRYDOWN_FIT         ? "DRYDOWN_FIT" :
             cal.state == CAL_NORMAL_OPERATION    ? "NORMAL" : "UNKNOWN");
}

IrrigationStatus PhysicsEngine::getIrrigationStatus(float theta, float theta_fc,
                                                      float theta_refill,
                                                      float dryingRate) {
    IrrigationStatus s;

    if (theta_refill <= 0.0f) {
        strcpy(s.state,          "UNKNOWN");
        strcpy(s.recommendation, "Calibrating system...");
        strcpy(s.urgency,        "none");
        return s;
    }

    float h = CONFIG.refill_hysteresis;

    if (theta < theta_refill - h) {
        strcpy(s.state,          "REFILL");
        strcpy(s.recommendation, "Irrigate now - soil moisture critical");
        strcpy(s.urgency,        "high");
        return s;
    }

    if (dryingRate < -0.002f && theta < theta_fc * 0.9f) {
        strcpy(s.state,          "MONITOR");
        strcpy(s.recommendation, "Rapid drying - monitor closely");
        strcpy(s.urgency,        "medium");
        return s;
    }

    if (theta < theta_fc && theta >= theta_refill - h) {
        if (dryingRate < -0.0005f) {
            strcpy(s.state,          "MONITOR");
            strcpy(s.recommendation, "Drying trend detected - monitor closely");
            strcpy(s.urgency,        "medium");
        } else {
            strcpy(s.state,          "OPTIMAL");
            strcpy(s.recommendation, "Soil moisture optimal");
            strcpy(s.urgency,        "low");
        }
        return s;
    }

    if (theta >= theta_fc) {
        strcpy(s.state,          "FULL");
        strcpy(s.recommendation, "Soil moisture excellent - no irrigation needed");
        strcpy(s.urgency,        "none");
        return s;
    }

    strcpy(s.state,          "OPTIMAL");
    strcpy(s.recommendation, "Soil moisture optimal");
    strcpy(s.urgency,        "low");
    return s;
}

DrainageQuality PhysicsEngine::assessDrainageQuality() {
    DrainageQuality q;
    float kd = dynamicsModel.params.kd;
    q.kd = kd;

    if (kd < 0.01f) {
        strcpy(q.quality,        "poor");
        strcpy(q.message,        "Soil drains slowly - possible clay or compaction");
        strcpy(q.recommendation, "Consider tillage to improve drainage");
    } else if (kd > 0.15f) {
        strcpy(q.quality,        "excessive");
        strcpy(q.message,        "Soil drains very quickly - likely sandy");
        strcpy(q.recommendation, "Add organic matter to improve water retention");
    } else {
        strcpy(q.quality,        "good");
        strcpy(q.message,        "Soil drainage is healthy");
        strcpy(q.recommendation, "No drainage action needed");
    }
    return q;
}

CalibrationResult PhysicsEngine::getCalibrationState() {
    return autoCalibration.getCalibrationState();
}
