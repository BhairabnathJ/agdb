/**
 * AgriScan Physics Engine
 * 
 * Implements soil-water physics, auto-calibration, and decision logic
 * Based on: van Genuchten (1980), FAO-56, Celia et al. (1990)
 * 
 * @version 1.0.0
 * @author Alex Jamkatel, Unity Provisions
 */

// =============================================================================
// CONFIGURATION & CONSTANTS
// =============================================================================

const CONFIG = {
    // Pre-processing & QC
    dt_resample: 15 * 60,          // 15 minutes in seconds
    smooth_window: 60 * 60,         // 60 minutes in seconds
    spike_z_thresh: 6,              // Z-score threshold for spike detection
    stuck_eps: 0.001,               // m³/m³ - stuck sensor threshold
    stuck_hours: 24,                // hours before declaring stuck
    theta_bounds: [0, 0.50],        // [min, max] VWC bounds (m³/m³)

    // Temperature compensation
    T_ref: 20,                      // °C - reference temperature
    a_temp: 0,                      // Initial temp correction coefficient

    // Event detection
    wet_jump_thresh: 0.02,          // m³/m³ - minimum jump for wetting event
    wet_window: 2 * 3600,           // 2 hours in seconds
    min_event_separation: 12 * 3600, // 12 hours in seconds
    post_event_ignore: 1 * 3600,    // 1 hour in seconds

    // FC* detection
    slope_window: 2 * 3600,         // 2 hours for slope calculation
    s_min: 0.0005,                  // m³/m³/hr - drainage ended threshold
    hold_hours: 8,                  // hours to confirm FC plateau
    fc_update_lambda: 0.25,         // EWMA weight for FC updates

    // Refill threshold
    theta_dry_percentile: 0.05,     // 5th percentile for dry baseline
    theta_dry_window: 30,           // days for rolling window
    eta_refill: 0.5,                // Refill fraction
    refill_hysteresis: 0.01,        // m³/m³ - prevent state flapping

    // Dynamics model
    kd_init: 0.05,                  // hr⁻¹ - initial drainage rate
    ku_init: 0.0005,                // initial drydown coefficient
    beta_init: 1.0,                 // drydown nonlinearity

    // Confidence scoring weights
    confidence_weights: {
        n_good_events: 0.40,
        fc_stability: 0.25,
        qc_pass_rate: 0.20,
        fit_residual: 0.15
    }
};

// Soil model defaults (loam - middle ground)
const DEFAULT_SOIL = {
    theta_r: 0.078,    // m³/m³ - residual water content
    theta_s: 0.43,     // m³/m³ - saturated water content  
    alpha: 0.036,      // cm⁻¹ - van Genuchten parameter
    n: 1.56,           // dimensionless - van Genuchten parameter
    Ks: 25.0,          // cm/day - saturated hydraulic conductivity
    // Computed
    m: null,           // = 1 - 1/n
    theta_fc: null,    // Field capacity (will be auto-calibrated)
    theta_pwp: null    // Permanent wilting point
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * Linear interpolation
 */
function lerp(a, b, t) {
    return a + (b - a) * t;
}

/**
 * Clamp value between min and max
 */
function clamp(val, min, max) {
    return Math.min(Math.max(val, min), max);
}

/**
 * Calculate median of array
 */
function median(arr) {
    if (arr.length === 0) return null;
    const sorted = [...arr].sort((a, b) => a - b);
    const mid = Math.floor(sorted.length / 2);
    return sorted.length % 2 === 0
        ? (sorted[mid - 1] + sorted[mid]) / 2
        : sorted[mid];
}

/**
 * Calculate percentile of array
 */
function percentile(arr, p) {
    if (arr.length === 0) return null;
    const sorted = [...arr].sort((a, b) => a - b);
    const index = (p / 100) * (sorted.length - 1);
    const lower = Math.floor(index);
    const upper = Math.ceil(index);
    const weight = index - lower;
    return sorted[lower] * (1 - weight) + sorted[upper] * weight;
}

/**
 * Calculate standard deviation
 */
function std(arr) {
    if (arr.length === 0) return 0;
    const mean = arr.reduce((a, b) => a + b, 0) / arr.length;
    const variance = arr.reduce((sum, val) => sum + Math.pow(val - mean, 2), 0) / arr.length;
    return Math.sqrt(variance);
}

/**
 * Exponentially weighted moving average
 */
function ewma(current, newValue, lambda) {
    if (current === null) return newValue;
    return (1 - lambda) * current + lambda * newValue;
}

// =============================================================================
// SENSOR CALIBRATION MODULE
// =============================================================================

class SensorCalibration {
    constructor() {
        this.factoryCurve = this.defaultFactoryCurve();
        this.siteCorrection = { offset: 0, gain: 1.0 };
        this.tempCorrection = CONFIG.a_temp;
    }

    /**
     * Default factory calibration curve for capacitive sensors
     * Maps raw ADC reading to VWC (m³/m³)
     * This is a generic curve - should be replaced with sensor-specific data
     */
    defaultFactoryCurve() {
        return {
            // Common capacitive sensor calibration points
            // Format: [raw_value, theta_m3m3]
            points: [
                [250, 0.00],   // Dry air
                [450, 0.10],   // Dry soil
                [650, 0.25],   // Moist soil
                [850, 0.40],   // Wet soil
                [1000, 0.50]   // Saturated
            ]
        };
    }

    /**
     * Convert raw sensor reading to VWC using factory calibration
     * Uses piecewise linear interpolation
     */
    rawToVWC(raw) {
        const points = this.factoryCurve.points;

        // Clamp to bounds
        if (raw <= points[0][0]) return points[0][1];
        if (raw >= points[points.length - 1][0]) return points[points.length - 1][1];

        // Find interpolation segment
        for (let i = 1; i < points.length; i++) {
            if (raw <= points[i][0]) {
                const x0 = points[i - 1][0], y0 = points[i - 1][1];
                const x1 = points[i][0], y1 = points[i][1];
                const t = (raw - x0) / (x1 - x0);
                return lerp(y0, y1, t);
            }
        }

        return points[points.length - 1][1];
    }

    /**
     * Apply temperature correction to VWC reading
     * Based on: θ_corrected = θ0 + a_temp * (T - T_ref)
     */
    temperatureCorrection(theta, temp_c) {
        return theta + this.tempCorrection * (temp_c - CONFIG.T_ref);
    }

    /**
     * Apply site-specific calibration correction
     */
    siteCorrection_apply(theta) {
        return this.siteCorrection.gain * theta + this.siteCorrection.offset;
    }

    /**
     * Full calibration pipeline: raw → VWC with all corrections
     */
    calibrate(raw, temp_c) {
        let theta = this.rawToVWC(raw);
        theta = this.siteCorrection_apply(theta);
        theta = this.temperatureCorrection(theta, temp_c);

        // Enforce physical bounds
        theta = clamp(theta, CONFIG.theta_bounds[0], CONFIG.theta_bounds[1]);

        return theta;
    }

    /**
     * Quality control checks on reading
     * Returns: { valid: boolean, flags: array }
     */
    qualityControl(theta, temp_c, history = []) {
        const flags = [];

        // Check physical bounds
        if (theta < CONFIG.theta_bounds[0] || theta > CONFIG.theta_bounds[1]) {
            flags.push('OUT_OF_BOUNDS');
        }

        // Check for spikes (if we have history)
        if (history.length >= 3) {
            const recent = history.slice(-5).map(h => h.theta);
            const mean = recent.reduce((a, b) => a + b, 0) / recent.length;
            const stdDev = std(recent);
            const z = Math.abs((theta - mean) / (stdDev + 0.001)); // Add small epsilon

            if (z > CONFIG.spike_z_thresh) {
                flags.push('SPIKE_DETECTED');
            }
        }

        // Check if sensor is stuck (if we have recent history)
        if (history.length >= 10) {
            const recent = history.slice(-10).map(h => h.theta);
            const range = Math.max(...recent) - Math.min(...recent);

            if (range < CONFIG.stuck_eps) {
                flags.push('SENSOR_STUCK');
            }
        }

        // Temperature sanity check (reasonable agricultural range)
        if (temp_c < -10 || temp_c > 60) {
            flags.push('TEMP_OUT_OF_RANGE');
        }

        return {
            valid: flags.length === 0,
            flags: flags
        };
    }
}

// =============================================================================
// SOIL HYDRAULIC MODEL MODULE
// =============================================================================

class SoilModel {
    constructor(params = DEFAULT_SOIL) {
        this.params = { ...params };
        this.params.m = 1 - (1 / this.params.n); // van Genuchten m parameter

        // Calculate default FC/PWP using van Genuchten at standard potentials
        // FC typically at -33 kPa (pF 2.5), PWP at -1500 kPa (pF 4.2)
        this.params.theta_fc = this.vanGenuchten_theta(330); // 33 kPa = 330 cm H2O
        this.params.theta_pwp = this.vanGenuchten_theta(15000); // 1500 kPa = 15000 cm
    }

    /**
     * van Genuchten water retention curve
     * Converts matric potential (ψ in cm H2O) to volumetric water content (θ)
     * 
     * θ(ψ) = θr + (θs - θr) / [1 + (α|ψ|)^n]^m
     * where m = 1 - 1/n
     * 
     * Reference: van Genuchten, M.Th. (1980). Soil Sci. Soc. Am. J. 44:892-898
     */
    vanGenuchten_theta(psi_cm) {
        const { theta_r, theta_s, alpha, n, m } = this.params;

        if (psi_cm <= 0) return theta_s; // Saturated

        const term = 1 + Math.pow(alpha * psi_cm, n);
        const theta = theta_r + (theta_s - theta_r) / Math.pow(term, m);

        return theta;
    }

    /**
     * Inverse van Genuchten: θ → ψ
     * Calculates matric potential (cm H2O) from volumetric water content
     * 
     * ψ(θ) = (1/α) * [ ((θs - θr)/(θ - θr))^(1/m) - 1 ]^(1/n)
     */
    vanGenuchten_psi(theta) {
        const { theta_r, theta_s, alpha, n, m } = this.params;

        // Clamp theta to valid range
        theta = clamp(theta, theta_r + 0.001, theta_s - 0.001);

        const Se = (theta - theta_r) / (theta_s - theta_r); // Effective saturation
        const term = Math.pow(1 / Se, 1 / m) - 1;
        const psi = Math.pow(term, 1 / n) / alpha;

        return psi; // cm H2O (positive value)
    }

    /**
     * Calculate relative saturation (effective saturation)
     * Se = (θ - θr) / (θs - θr)
     */
    effectiveSaturation(theta) {
        const { theta_r, theta_s } = this.params;
        return clamp((theta - theta_r) / (theta_s - theta_r), 0, 1);
    }

    /**
     * Calculate plant-available water metrics
     * Returns object with TAW, AW, depletion info
     */
    availableWater(theta, rootDepth_cm = 30) {
        const { theta_fc, theta_pwp } = this.params;

        // Total Available Water (mm of water in root zone)
        const TAW = (theta_fc - theta_pwp) * rootDepth_cm * 10; // mm

        // Available Water (current)
        const AW = Math.max(0, (theta - theta_pwp) * rootDepth_cm * 10); // mm

        // Depletion
        const Dr = TAW - AW; // mm depleted
        const fractionDepleted = TAW > 0 ? Dr / TAW : 0;

        return {
            TAW_mm: TAW,
            AW_mm: AW,
            Dr_mm: Dr,
            fractionDepleted: clamp(fractionDepleted, 0, 1),
            theta_fc: theta_fc,
            theta_pwp: theta_pwp
        };
    }

    /**
     * Hydraulic conductivity using Mualem-van Genuchten model
     * K(θ) = Ks * Se^L * [1 - (1 - Se^(1/m))^m]^2
     * where L ≈ 0.5 (pore connectivity parameter)
     * 
     * Reference: van Genuchten (1980), Mualem (1976)
     */
    hydraulicConductivity(theta) {
        const { Ks } = this.params;
        const Se = this.effectiveSaturation(theta);
        const L = 0.5; // Mualem pore connectivity
        const m = this.params.m;

        if (Se >= 1.0) return Ks;
        if (Se <= 0.01) return Ks * 1e-10; // Near zero

        const term = 1 - Math.pow(1 - Math.pow(Se, 1 / m), m);
        const K = Ks * Math.pow(Se, L) * Math.pow(term, 2);

        return K; // cm/day
    }
}

// =============================================================================
// EVENT DETECTION MODULE
// =============================================================================

class EventDetection {
    constructor() {
        this.lastEventTime = null;
        this.currentEvent = null;
        this.eventHistory = [];
    }

    /**
     * Detect wetting event (rain or irrigation)
     * Returns: { detected: boolean, delta_theta: number, confidence: string }
     */
    detectWetting(history) {
        if (history.length < 2) {
            return { detected: false, delta_theta: 0, confidence: 'insufficient_data' };
        }

        // Get time window
        const now = history[history.length - 1].timestamp;
        const windowStart = now - CONFIG.wet_window;
        const recentData = history.filter(h => h.timestamp >= windowStart);

        if (recentData.length < 2) {
            return { detected: false, delta_theta: 0, confidence: 'insufficient_data' };
        }

        // Calculate moisture jump
        const thetaStart = recentData[0].theta;
        const thetaEnd = recentData[recentData.length - 1].theta;
        const deltaTheta = thetaEnd - thetaStart;

        // Check if jump exceeds threshold
        if (deltaTheta >= CONFIG.wet_jump_thresh) {
            // Check minimum separation from last event
            if (this.lastEventTime && (now - this.lastEventTime) < CONFIG.min_event_separation) {
                return {
                    detected: false,
                    delta_theta: deltaTheta,
                    confidence: 'too_soon_after_last_event'
                };
            }

            this.lastEventTime = now;
            return {
                detected: true,
                delta_theta: deltaTheta,
                confidence: 'high'
            };
        }

        return { detected: false, delta_theta: deltaTheta, confidence: 'below_threshold' };
    }

    /**
     * Calculate moisture change rate (dθ/dt)
     * Uses linear regression over specified window
     */
    calculateDryingRate(history, windowSeconds = CONFIG.slope_window) {
        if (history.length < 3) return null;

        const now = history[history.length - 1].timestamp;
        const windowStart = now - windowSeconds;
        const windowData = history.filter(h => h.timestamp >= windowStart);

        if (windowData.length < 3) return null;

        // Simple linear regression: dθ/dt
        const n = windowData.length;
        const t0 = windowData[0].timestamp;

        let sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        windowData.forEach(point => {
            const x = (point.timestamp - t0) / 3600; // Convert to hours
            const y = point.theta;
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
        });

        const slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);

        return slope; // m³/m³/hr
    }

    /**
     * Detect drainage plateau (field capacity proxy)
     * Returns: { detected: boolean, theta_fc_candidate: number, confidence: string }
     */
    detectFCPlateau(history) {
        if (history.length < 20) {
            return { detected: false, theta_fc_candidate: null, confidence: 'insufficient_data' };
        }

        const dryingRate = this.calculateDryingRate(history);

        if (dryingRate === null) {
            return { detected: false, theta_fc_candidate: null, confidence: 'cannot_calculate_rate' };
        }

        // Check if rate is below threshold
        if (Math.abs(dryingRate) < CONFIG.s_min) {
            // Check if this has been sustained for hold_hours
            const holdSeconds = CONFIG.hold_hours * 3600;
            const now = history[history.length - 1].timestamp;
            const plateauStart = now - holdSeconds;
            const plateauData = history.filter(h => h.timestamp >= plateauStart);

            if (plateauData.length < 10) {
                return { detected: false, theta_fc_candidate: null, confidence: 'plateau_too_short' };
            }

            // Calculate plateau theta (median for robustness)
            const thetaValues = plateauData.map(h => h.theta);
            const theta_fc_candidate = median(thetaValues);

            return {
                detected: true,
                theta_fc_candidate: theta_fc_candidate,
                confidence: 'high'
            };
        }

        return {
            detected: false,
            theta_fc_candidate: null,
            confidence: 'still_draining'
        };
    }

    /**
     * Classify current dynamics regime
     * Returns: 'wetting' | 'drainage' | 'drydown' | 'stable'
     */
    classifyRegime(history, theta_fc = null) {
        if (history.length < 5) return 'unknown';

        const dryingRate = this.calculateDryingRate(history);
        if (dryingRate === null) return 'unknown';

        const currentTheta = history[history.length - 1].theta;

        // Wetting: rapid increase
        if (dryingRate > 0.001) return 'wetting';

        // Stable: very slow change
        if (Math.abs(dryingRate) < CONFIG.s_min) return 'stable';

        // Drainage vs drydown depends on FC
        if (theta_fc !== null) {
            if (currentTheta > theta_fc) {
                return 'drainage'; // Above FC, gravity drainage
            } else {
                return 'drydown'; // Below FC, ET-driven
            }
        }

        // Default to drydown if FC not known
        return dryingRate < 0 ? 'drydown' : 'stable';
    }
}

// =============================================================================
// DYNAMICS MODEL MODULE
// =============================================================================

class DynamicsModel {
    constructor() {
        this.params = {
            kd: CONFIG.kd_init,      // Drainage rate (hr⁻¹)
            ku: CONFIG.ku_init,      // Drydown coefficient
            beta: CONFIG.beta_init,  // Drydown nonlinearity
            theta_min: 0.05          // Minimum theta for drydown model
        };
    }

    /**
     * Drainage regime model: θ(t) = θ_FC + (θ0 - θ_FC) * exp(-kd * t)
     * Or as rate: dθ/dt = -kd * (θ - θ_FC)
     */
    drainageRate(theta, theta_fc) {
        if (theta <= theta_fc) return 0;
        return -this.params.kd * (theta - theta_fc); // m³/m³/hr
    }

    /**
     * Drydown regime model: dθ/dt = -ku * (θ - θ_min)^β
     */
    drydownRate(theta) {
        if (theta <= this.params.theta_min) return 0;
        const rate = -this.params.ku * Math.pow(theta - this.params.theta_min, this.params.beta);
        return rate; // m³/m³/hr
    }

    /**
     * Fit drainage parameter kd from observed data
     * Uses exponential fit on drainage segment
     */
    fitDrainageParameter(drainageData, theta_fc) {
        if (drainageData.length < 5) return null;

        // Filter to points above FC
        const validPoints = drainageData.filter(p => p.theta > theta_fc);
        if (validPoints.length < 5) return null;

        // Linearize: ln(θ - θ_FC) = ln(θ0 - θ_FC) - kd * t
        const t0 = validPoints[0].timestamp;
        const theta0 = validPoints[0].theta;

        let sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, n = 0;

        validPoints.forEach(point => {
            const t = (point.timestamp - t0) / 3600; // hours
            const deltaTheta = point.theta - theta_fc;

            if (deltaTheta > 0.001) { // Avoid log of very small numbers
                const x = t;
                const y = Math.log(deltaTheta);

                sumX += x;
                sumY += y;
                sumXY += x * y;
                sumX2 += x * x;
                n++;
            }
        });

        if (n < 5) return null;

        // Linear regression slope = -kd
        const slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
        const kd = -slope; // Make positive

        // Sanity check
        if (kd < 0.001 || kd > 1.0) return null;

        return kd;
    }

    /**
     * Fit drydown parameters (ku, beta) from observed data
     * This is more complex - simplified version here
     */
    fitDrydownParameters(drydownData) {
        if (drydownData.length < 10) return null;

        // For now, use simple exponential-like fit (beta = 1)
        // More sophisticated fitting would use nonlinear optimization

        const t0 = drydownData[0].timestamp;
        const theta0 = drydownData[0].theta;
        const theta_end = drydownData[drydownData.length - 1].theta;
        const t_end = (drydownData[drydownData.length - 1].timestamp - t0) / 3600; // hours

        // Estimate theta_min as minimum observed
        const theta_min = Math.min(...drydownData.map(p => p.theta)) - 0.01;

        // For beta = 1: θ(t) ≈ θ_min + (θ0 - θ_min) * exp(-ku * t)
        // Solve for ku
        if (theta_end > theta_min && theta0 > theta_min && t_end > 0) {
            const ku = -Math.log((theta_end - theta_min) / (theta0 - theta_min)) / t_end;

            if (ku > 0 && ku < 0.1) {
                return { ku: ku, beta: 1.0, theta_min: theta_min };
            }
        }

        return null;
    }
}

// =============================================================================
// AUTO-CALIBRATION MODULE
// =============================================================================

class AutoCalibration {
    constructor(soilModel, eventDetection, dynamicsModel) {
        this.soilModel = soilModel;
        this.eventDetection = eventDetection;
        this.dynamicsModel = dynamicsModel;

        this.state = 'INIT'; // State machine state
        this.theta_fc_star = null; // Auto-calibrated field capacity
        this.theta_refill_star = null; // Auto-calibrated refill point
        this.fc_history = []; // Track FC estimates for stability
        this.confidence = 0.2; // Initial confidence score
        this.stats = {
            n_events: 0,
            n_fc_updates: 0,
            qc_pass_count: 0,
            qc_total_count: 0
        };
    }

    /**
     * Update calibration with new data point
     * This is called for each new sensor reading
     */
    update(dataPoint, history) {
        this.stats.qc_total_count++;
        if (dataPoint.qc_valid) {
            this.stats.qc_pass_count++;
        }

        // State machine for calibration
        switch (this.state) {
            case 'INIT':
                this.state_init(history);
                break;

            case 'BASELINE_MONITORING':
                this.state_baseline(dataPoint, history);
                break;

            case 'WETTING_EVENT':
                this.state_wetting(dataPoint, history);
                break;

            case 'DRAINAGE_TRACKING':
                this.state_drainage(dataPoint, history);
                break;

            case 'FC_ESTIMATE':
                this.state_fc_estimate(dataPoint, history);
                break;

            case 'DRYDOWN_FIT':
                this.state_drydown_fit(dataPoint, history);
                break;

            case 'NORMAL_OPERATION':
                this.state_normal(dataPoint, history);
                break;
        }

        // Update confidence score
        this.updateConfidence();
    }

    state_init(history) {
        // Collect baseline data for at least 24 hours
        if (history.length < 96) return; // 96 samples at 15-min = 24 hours

        // Initialize theta_fc_star with van Genuchten default
        this.theta_fc_star = this.soilModel.params.theta_fc;

        // Estimate theta_dry_min from history
        const thetaValues = history.map(h => h.theta);
        const theta_dry = percentile(thetaValues, CONFIG.theta_dry_percentile * 100);

        // Calculate initial refill threshold
        this.theta_refill_star = this.theta_fc_star -
            CONFIG.eta_refill * (this.theta_fc_star - theta_dry);

        this.state = 'BASELINE_MONITORING';
    }

    state_baseline(dataPoint, history) {
        // Look for wetting events
        const wetting = this.eventDetection.detectWetting(history);

        if (wetting.detected) {
            this.stats.n_events++;
            this.state = 'WETTING_EVENT';
            this.currentEventStart = dataPoint.timestamp;
        }
    }

    state_wetting(dataPoint, history) {
        // Wait for post-event ignore period
        const timeSinceEvent = dataPoint.timestamp - this.currentEventStart;

        if (timeSinceEvent > CONFIG.post_event_ignore) {
            this.state = 'DRAINAGE_TRACKING';
        }
    }

    state_drainage(dataPoint, history) {
        // Look for FC plateau
        const plateau = this.eventDetection.detectFCPlateau(history);

        if (plateau.detected) {
            this.state = 'FC_ESTIMATE';
            this.fc_candidate = plateau.theta_fc_candidate;
        }

        // If we start seeing drydown without finding FC, give up on this event
        const regime = this.eventDetection.classifyRegime(history, this.theta_fc_star);
        if (regime === 'drydown') {
            this.state = 'NORMAL_OPERATION';
        }
    }

    state_fc_estimate(dataPoint, history) {
        // Update FC* using EWMA
        if (this.theta_fc_star === null) {
            this.theta_fc_star = this.fc_candidate;
        } else {
            this.theta_fc_star = ewma(
                this.theta_fc_star,
                this.fc_candidate,
                CONFIG.fc_update_lambda
            );
        }

        this.fc_history.push(this.theta_fc_star);
        this.stats.n_fc_updates++;

        // Update refill threshold
        this.updateRefillThreshold(history);

        // Try to fit drainage parameters
        const drainageData = this.extractDrainageSegment(history);
        if (drainageData && drainageData.length >= 5) {
            const kd = this.dynamicsModel.fitDrainageParameter(drainageData, this.theta_fc_star);
            if (kd !== null) {
                this.dynamicsModel.params.kd = kd;
            }
        }

        this.state = 'DRYDOWN_FIT';
    }

    state_drydown_fit(dataPoint, history) {
        // Look for drydown segment to fit parameters
        const regime = this.eventDetection.classifyRegime(history, this.theta_fc_star);

        if (regime === 'drydown') {
            const drydownData = this.extractDrydownSegment(history);
            if (drydownData && drydownData.length >= 10) {
                const params = this.dynamicsModel.fitDrydownParameters(drydownData);
                if (params !== null) {
                    this.dynamicsModel.params.ku = params.ku;
                    this.dynamicsModel.params.beta = params.beta;
                    this.dynamicsModel.params.theta_min = params.theta_min;
                }
            }
        }

        this.state = 'NORMAL_OPERATION';
    }

    state_normal(dataPoint, history) {
        // Continue monitoring for new events
        const wetting = this.eventDetection.detectWetting(history);

        if (wetting.detected) {
            this.stats.n_events++;
            this.state = 'WETTING_EVENT';
            this.currentEventStart = dataPoint.timestamp;
        }
    }

    updateRefillThreshold(history) {
        // Calculate theta_dry_min from recent history
        const daySeconds = 86400;
        const windowSeconds = CONFIG.theta_dry_window * daySeconds;
        const now = history[history.length - 1].timestamp;
        const windowStart = now - windowSeconds;
        const recentData = history.filter(h => h.timestamp >= windowStart);

        if (recentData.length > 100) {
            const thetaValues = recentData.map(h => h.theta);
            const theta_dry = percentile(thetaValues, CONFIG.theta_dry_percentile * 100);

            this.theta_refill_star = this.theta_fc_star -
                CONFIG.eta_refill * (this.theta_fc_star - theta_dry);
        }
    }

    extractDrainageSegment(history) {
        // Find most recent segment where regime = 'drainage'
        // This is simplified - would need more sophisticated event segmentation
        const drainageData = [];

        for (let i = history.length - 1; i >= 0; i--) {
            const regime = this.eventDetection.classifyRegime(
                history.slice(0, i + 1),
                this.theta_fc_star
            );

            if (regime === 'drainage') {
                drainageData.unshift(history[i]);
            } else if (drainageData.length > 0) {
                break; // Found end of drainage segment
            }
        }

        return drainageData.length >= 5 ? drainageData : null;
    }

    extractDrydownSegment(history) {
        // Find most recent segment where regime = 'drydown'
        const drydownData = [];

        for (let i = history.length - 1; i >= 0; i--) {
            const regime = this.eventDetection.classifyRegime(
                history.slice(0, i + 1),
                this.theta_fc_star
            );

            if (regime === 'drydown') {
                drydownData.unshift(history[i]);
            } else if (drydownData.length > 0) {
                break;
            }
        }

        return drydownData.length >= 10 ? drydownData : null;
    }

    updateConfidence() {
        const w = CONFIG.confidence_weights;

        // Component 1: Number of good events (saturates at 5-8 events)
        const eventScore = Math.min(this.stats.n_events / 8, 1.0);

        // Component 2: FC stability (lower stddev = higher score)
        let stabilityScore = 0.5;
        if (this.fc_history.length >= 3) {
            const fc_std = std(this.fc_history);
            stabilityScore = Math.exp(-fc_std / 0.02); // Decays with stddev
        }

        // Component 3: QC pass rate
        const qcScore = this.stats.qc_total_count > 0
            ? this.stats.qc_pass_count / this.stats.qc_total_count
            : 0.5;

        // Component 4: Fit residual quality (placeholder - would need actual residuals)
        const fitScore = 0.7; // Assume decent fit for now

        // Weighted sum
        this.confidence =
            w.n_good_events * eventScore +
            w.fc_stability * stabilityScore +
            w.qc_pass_rate * qcScore +
            w.fit_residual * fitScore;

        this.confidence = clamp(this.confidence, 0, 1);
    }

    getCalibrationState() {
        return {
            state: this.state,
            theta_fc_star: this.theta_fc_star,
            theta_refill_star: this.theta_refill_star,
            confidence: this.confidence,
            n_events: this.stats.n_events,
            n_fc_updates: this.stats.n_fc_updates,
            qc_pass_rate: this.stats.qc_total_count > 0
                ? this.stats.qc_pass_count / this.stats.qc_total_count
                : 0,
            dynamics_params: this.dynamicsModel.params
        };
    }
}

// =============================================================================
// MAIN PHYSICS ENGINE
// =============================================================================

class PhysicsEngine {
    constructor(soilParams = DEFAULT_SOIL) {
        this.calibration = new SensorCalibration();
        this.soilModel = new SoilModel(soilParams);
        this.eventDetection = new EventDetection();
        this.dynamicsModel = new DynamicsModel();
        this.autoCalibration = new AutoCalibration(
            this.soilModel,
            this.eventDetection,
            this.dynamicsModel
        );

        this.history = []; // Store processed data points
        this.maxHistoryLength = 2880; // 30 days at 15-min intervals
    }

    /**
     * Process new sensor reading
     * This is the main entry point
     */
    processSensorReading(raw, temp_c, timestamp = Date.now()) {
        // Step 1: Calibrate raw reading to VWC
        const theta = this.calibration.calibrate(raw, temp_c);

        // Step 2: Quality control
        const qc = this.calibration.qualityControl(theta, temp_c, this.history);

        // Step 3: Create data point
        const dataPoint = {
            timestamp: timestamp,
            raw: raw,
            temp_c: temp_c,
            theta: theta,
            qc_valid: qc.valid,
            qc_flags: qc.flags
        };

        // Step 4: Add to history
        this.history.push(dataPoint);
        if (this.history.length > this.maxHistoryLength) {
            this.history.shift(); // Remove oldest
        }

        // Step 5: Update auto-calibration
        if (qc.valid) {
            this.autoCalibration.update(dataPoint, this.history);
        }

        // Step 6: Calculate derived metrics
        const metrics = this.calculateMetrics(dataPoint);

        return {
            ...dataPoint,
            ...metrics
        };
    }

    /**
     * Calculate all derived metrics for current state
     */
    calculateMetrics(dataPoint) {
        const { theta, temp_c } = dataPoint;

        // Get calibration state
        const calState = this.autoCalibration.getCalibrationState();
        const theta_fc = calState.theta_fc_star || this.soilModel.params.theta_fc;
        const theta_refill = calState.theta_refill_star;

        // Available water metrics
        const aw = this.soilModel.availableWater(theta);

        // Matric potential
        const psi_cm = this.soilModel.vanGenuchten_psi(theta);
        const psi_kPa = psi_cm / 10; // Convert to kPa

        // Effective saturation
        const Se = this.soilModel.effectiveSaturation(theta);

        // Hydraulic conductivity
        const K = this.soilModel.hydraulicConductivity(theta);

        // Drying rate
        const dryingRate = this.eventDetection.calculateDryingRate(this.history);

        // Regime classification
        const regime = this.eventDetection.classifyRegime(this.history, theta_fc);

        // Irrigation status
        const status = this.getIrrigationStatus(theta, theta_fc, theta_refill, dryingRate);

        return {
            // Soil state
            theta_fc: theta_fc,
            theta_refill: theta_refill,
            psi_cm: psi_cm,
            psi_kPa: psi_kPa,
            Se: Se,
            K_cm_day: K,

            // Available water
            TAW_mm: aw.TAW_mm,
            AW_mm: aw.AW_mm,
            Dr_mm: aw.Dr_mm,
            fractionDepleted: aw.fractionDepleted,

            // Dynamics
            dryingRate_per_hr: dryingRate,
            regime: regime,

            // Status
            status: status.state,
            recommendation: status.recommendation,
            urgency: status.urgency,

            // Calibration
            confidence: calState.confidence,
            calibration_state: calState.state
        };
    }

    /**
     * Determine irrigation status and recommendations
     */
    getIrrigationStatus(theta, theta_fc, theta_refill, dryingRate) {
        if (!theta_refill) {
            return {
                state: 'UNKNOWN',
                recommendation: 'Calibrating system...',
                urgency: 'none'
            };
        }

        // Apply hysteresis to prevent flapping
        const hysteresis = CONFIG.refill_hysteresis;

        // Critical: Below refill point OR rapid drying
        if (theta < theta_refill - hysteresis ||
            (dryingRate && dryingRate < -0.002)) { // -0.002 m³/m³/hr = rapid
            return {
                state: 'REFILL',
                recommendation: 'Irrigate now - soil moisture critical',
                urgency: 'high'
            };
        }

        // Warning: Approaching refill point OR moderate drying
        if (theta < theta_fc && theta >= theta_refill - hysteresis) {
            if (dryingRate && dryingRate < -0.0005) {
                return {
                    state: 'MONITOR',
                    recommendation: 'Monitor closely - drying trend detected',
                    urgency: 'medium'
                };
            }
            return {
                state: 'OPTIMAL',
                recommendation: 'Soil moisture optimal',
                urgency: 'low'
            };
        }

        // Good: Above FC and stable
        if (theta >= theta_fc) {
            return {
                state: 'FULL',
                recommendation: 'Soil moisture excellent - no irrigation needed',
                urgency: 'none'
            };
        }

        return {
            state: 'OPTIMAL',
            recommendation: 'Soil moisture optimal',
            urgency: 'low'
        };
    }

    /**
     * Get complete system status
     */
    getSystemStatus() {
        if (this.history.length === 0) {
            return {
                status: 'NO_DATA',
                message: 'No sensor data available'
            };
        }

        const latest = this.history[this.history.length - 1];
        const metrics = this.calculateMetrics(latest);

        return {
            status: 'RUNNING',
            latest: latest,
            metrics: metrics,
            calibration: this.autoCalibration.getCalibrationState(),
            history_length: this.history.length
        };
    }

    /**
     * Export history for external analysis
     */
    exportHistory() {
        return this.history.map(point => ({
            timestamp: new Date(point.timestamp).toISOString(),
            theta: point.theta.toFixed(4),
            temp_c: point.temp_c.toFixed(2),
            qc_valid: point.qc_valid,
            qc_flags: point.qc_flags.join(',')
        }));
    }
}

// =============================================================================
// EXPORTS
// =============================================================================

// For Node.js/CommonJS
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        PhysicsEngine,
        SensorCalibration,
        SoilModel,
        EventDetection,
        DynamicsModel,
        AutoCalibration,
        CONFIG,
        DEFAULT_SOIL
    };
}

// For browser/ES6
if (typeof window !== 'undefined') {
    window.AgriScanPhysics = {
        PhysicsEngine,
        SensorCalibration,
        SoilModel,
        EventDetection,
        DynamicsModel,
        AutoCalibration,
        CONFIG,
        DEFAULT_SOIL
    };
}