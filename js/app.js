/**
 * AgriScan App - Full Feature Implementation
 *
 * Features:
 * - Tier 1/2/3 views with proper translations
 * - Zone mapping with multi-sensor simulation
 * - Comprehensive simulations (Rain, Drought, 10-min Long Run)
 * - Full console logging for debugging
 * - Data export with visualization support
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

const THRESHOLDS_STORE = {
    data: null,

    async load() {
        if (this.data) return this.data;
        const res = await fetch('config/crop_thresholds.json');
        if (!res.ok) throw new Error('Failed to load crop_thresholds.json');
        this.data = await res.json();
        return this.data;
    },

    getCrop(cropKey) {
        return this.data?.crops?.[cropKey] || null;
    },

    getSoil(soilKey) {
        return this.data?.soils?.[soilKey] || null;
    },

    getCropStage(cropKey, plantingTs) {
        const crop = this.getCrop(cropKey);
        if (!crop || !Array.isArray(crop.stages) || crop.stages.length === 0) return null;

        const startTs = Number(plantingTs || 0);
        const days = startTs > 0 ? Math.max(0, Math.floor((Date.now() / 1000 - startTs) / 86400)) : 0;
        return crop.stages.find((stage) => days >= stage.day_start && days <= stage.day_end)
            || crop.stages[crop.stages.length - 1];
    },

    getThresholds(cropKey, soilKey, plantingTs) {
        const soil = this.getSoil(soilKey);
        const stage = this.getCropStage(cropKey, plantingTs);
        if (!soil || !stage) return null;

        const thetaFc = Number(soil.theta_fc);
        const thetaWp = Number(soil.theta_wp);
        const p = Number(stage.p);
        const thetaRefill = thetaFc - p * (thetaFc - thetaWp);

        return {
            theta_fc: thetaFc,
            theta_wp: thetaWp,
            p,
            theta_refill: thetaRefill
        };
    }
};

// Zone configuration for multi-sensor simulation
const ZONE_CONFIG = {
    rows: 4,
    cols: 4,
    sensors: [
        { id: 'A1', row: 0, col: 0, active: true },
        { id: 'A2', row: 0, col: 1, active: true },
        { id: 'A3', row: 0, col: 2, active: false },
        { id: 'A4', row: 0, col: 3, active: true },
        { id: 'B1', row: 1, col: 0, active: true },
        { id: 'B2', row: 1, col: 1, active: false },
        { id: 'B3', row: 1, col: 2, active: true },
        { id: 'B4', row: 1, col: 3, active: false },
        { id: 'C1', row: 2, col: 0, active: false },
        { id: 'C2', row: 2, col: 1, active: true },
        { id: 'C3', row: 2, col: 2, active: false },
        { id: 'C4', row: 2, col: 3, active: true },
        { id: 'D1', row: 3, col: 0, active: false },
        { id: 'D2', row: 3, col: 1, active: true },
        { id: 'D3', row: 3, col: 2, active: true },
        { id: 'D4', row: 3, col: 3, active: false }
    ]
};

// =============================================================================
// LOGGING SYSTEM
// =============================================================================

const Logger = {
    enabled: true,
    history: [],
    maxHistory: 1000,

    log: function (category, message, data = null) {
        if (!this.enabled) return;

        const timestamp = new Date().toISOString();
        const entry = { timestamp, category, message, data };

        this.history.push(entry);
        if (this.history.length > this.maxHistory) {
            this.history.shift();
        }

        const style = this.getStyle(category);
        if (data) {
            console.log(`%c[${category}] ${message}`, style, data);
        } else {
            console.log(`%c[${category}] ${message}`, style);
        }
    },

    getStyle: function (category) {
        const styles = {
            'PHYSICS': 'color: #2E7D32; font-weight: bold;',
            'SIMULATION': 'color: #1976D2; font-weight: bold;',
            'UI': 'color: #7B1FA2; font-weight: bold;',
            'DATA': 'color: #F57C00; font-weight: bold;',
            'ZONE': 'color: #00796B; font-weight: bold;',
            'I18N': 'color: #5D4037; font-weight: bold;',
            'ERROR': 'color: #C62828; font-weight: bold;',
            'CALIBRATION': 'color: #303F9F; font-weight: bold;'
        };
        return styles[category] || 'color: #666;';
    },

    exportLogs: function () {
        return JSON.stringify(this.history, null, 2);
    },

    clear: function () {
        this.history = [];
        console.clear();
        console.log('%c[Logger] Logs cleared', 'color: #999;');
    }
};

// =============================================================================
// PHYSICS EVENT LOGGING SYSTEM
// =============================================================================

const PhysicsEventLogger = {
    events: [],
    lastCalState: null,
    lastTheta: null,
    wettingThreshold: 0.02, // m³/m³ - minimum jump for wetting event

    /**
     * Log a physics event (called when calibration events occur)
     * Simulates POST /api/log_event to ESP32
     */
    logEvent: async function (eventType, details, calState) {
        const event = {
            timestamp: new Date().toISOString(),
            event_type: eventType,
            details: details,
            cal_state: calState
        };

        this.events.push(event);
        Logger.log('PHYSICS_EVENT', `${eventType}: ${details}`, { calState });

        // Simulate API call to ESP32 (in real ESP32, this writes to physics_events.csv)
        try {
            // MockAPI.logPhysicsEvent simulates the POST /api/log_event endpoint
            await MockAPI.logPhysicsEvent(event);
        } catch (error) {
            Logger.log('ERROR', 'Failed to log physics event', error);
        }

        return event;
    },

    /**
     * Check for physics events by comparing current state to previous
     * Called after each sensor reading
     */
    checkForEvents: function (sample) {
        if (!sample || typeof Physics === 'undefined') return;

        const calState = Physics.autoCalibration?.getCalibrationState();
        if (!calState) return;

        const currentState = calState.state;
        const theta = sample.theta;

        // Check for wetting event
        if (this.lastTheta !== null && theta - this.lastTheta >= this.wettingThreshold) {
            this.logEvent('WETTING_DETECTED', `delta_theta=${(theta - this.lastTheta).toFixed(3)}`, currentState);
        }

        // Check for calibration state change
        if (this.lastCalState !== null && this.lastCalState !== currentState) {
            this.logEvent('STATE_CHANGE', `${this.lastCalState}->${currentState}`, currentState);

            // Log specific events based on new state
            if (currentState === 'FC_ESTIMATE' && calState.theta_fc_star) {
                this.logEvent('FC_UPDATE', `theta_fc_star=${calState.theta_fc_star.toFixed(3)}`, currentState);
            }
            if (currentState === 'NORMAL_OPERATION' && calState.theta_refill_star) {
                this.logEvent('REFILL_UPDATE', `theta_refill_star=${calState.theta_refill_star.toFixed(3)}`, currentState);
            }
        }

        // Check for FC plateau detection (when transitioning to FC_ESTIMATE)
        if (currentState === 'FC_ESTIMATE' && this.lastCalState === 'DRAINAGE_TRACKING') {
            this.logEvent('FC_PLATEAU', `theta_fc=${calState.theta_fc_star?.toFixed(3)}`, currentState);
        }

        // Update tracking variables
        this.lastCalState = currentState;
        this.lastTheta = theta;
    },

    /**
     * Export events as CSV string
     */
    exportCSV: function () {
        let csv = 'timestamp,event_type,details,cal_state\n';
        this.events.forEach(e => {
            csv += `${e.timestamp},${e.event_type},"${e.details}",${e.cal_state}\n`;
        });
        return csv;
    },

    /**
     * Get all events
     */
    getEvents: function () {
        return this.events;
    },

    /**
     * Clear events
     */
    clear: function () {
        this.events = [];
        this.lastCalState = null;
        this.lastTheta = null;
    }
};

// =============================================================================
// MOCK API (Simulating ESP32 Web Server)
// =============================================================================

const MockAPI = {
    db: [],
    zones: {},
    simulationRunning: false,
    simulationInterval: null,
    simulationLogs: [],

    init: function () {
        // Initialize zones
        ZONE_CONFIG.sensors.forEach(sensor => {
            if (sensor.active) {
                this.zones[sensor.id] = {
                    ...sensor,
                    history: [],
                    currentRaw: 600 + Math.random() * 200,
                    battery: 70 + Math.random() * 30
                };
            }
        });
        Logger.log('DATA', `Initialized ${Object.keys(this.zones).length} active zones`);
    },

    inject: function (raw_adc, temp_c, zoneId = 'A1') {
        const ts = Math.floor(Date.now() / 1000);

        if (typeof Physics === 'undefined' || !Physics.processSensorReading) {
            Logger.log('ERROR', 'Physics engine not loaded or incompatible');
            return null;
        }

        const sample = Physics.processSensorReading(raw_adc, temp_c, ts);

        // Add zone info
        sample.zoneId = zoneId;
        sample.battery = this.zones[zoneId]?.battery || 100;

        // Log physics output
        Logger.log('PHYSICS', `Zone ${zoneId} processed`, {
            raw: raw_adc,
            theta: (sample.theta * 100).toFixed(2) + '%',
            status: sample.status,
            urgency: sample.urgency,
            confidence: (sample.confidence * 100).toFixed(1) + '%',
            regime: sample.regime,
            psi_kPa: sample.psi_kPa?.toFixed(2)
        });

        // Store in main DB
        this.db.push(sample);
        if (this.db.length > 1000) this.db.shift();

        // Store in zone history
        if (this.zones[zoneId]) {
            this.zones[zoneId].history.push(sample);
            this.zones[zoneId].currentRaw = raw_adc;
            if (this.zones[zoneId].history.length > 200) {
                this.zones[zoneId].history.shift();
            }
        }

        // Check for physics events (wetting, state changes, etc.)
        PhysicsEventLogger.checkForEvents(sample);

        return sample;
    },

    seed: function () {
        Logger.log('DATA', 'Seeding initial data for all zones...');
        let now = Math.floor(Date.now() / 1000);

        Object.keys(this.zones).forEach(zoneId => {
            let raw = 500 + Math.random() * 300;
            for (let i = 0; i < 30; i++) {
                const t = now - (30 - i) * 600;
                raw += (Math.random() - 0.48) * 30; // Slight drying trend
                raw = Math.max(300, Math.min(900, raw));

                if (typeof Physics !== 'undefined' && Physics.processSensorReading) {
                    const sample = Physics.processSensorReading(raw, 22 + Math.random() * 4, t);
                    sample.zoneId = zoneId;
                    sample.battery = this.zones[zoneId].battery;
                    this.db.push(sample);
                    this.zones[zoneId].history.push(sample);
                    this.zones[zoneId].currentRaw = raw;
                }
            }
        });

        Logger.log('DATA', `Seeded ${this.db.length} total samples across ${Object.keys(this.zones).length} zones`);
    },

    getCurrent: async function (zoneId = null) {
        if (this.db.length === 0) {
            this.seed();
        }

        if (zoneId && this.zones[zoneId]) {
            const history = this.zones[zoneId].history;
            return history.length > 0 ? history[history.length - 1] : null;
        }

        return this.db[this.db.length - 1];
    },

    getSeries: async function (zoneId = null) {
        if (zoneId && this.zones[zoneId]) {
            return this.zones[zoneId].history;
        }
        return this.db;
    },

    getAllZones: function () {
        const zoneStatus = {};
        Object.keys(this.zones).forEach(zoneId => {
            const history = this.zones[zoneId].history;
            const latest = history.length > 0 ? history[history.length - 1] : null;
            zoneStatus[zoneId] = {
                ...this.zones[zoneId],
                latest: latest
            };
        });
        return zoneStatus;
    },

    getGlobalStatus: function () {
        const zones = this.getAllZones();
        let worstUrgency = 'none';
        let criticalCount = 0;
        let warningCount = 0;

        Object.values(zones).forEach(zone => {
            if (zone.latest) {
                if (zone.latest.urgency === 'high') {
                    worstUrgency = 'high';
                    criticalCount++;
                } else if (zone.latest.urgency === 'medium' && worstUrgency !== 'high') {
                    worstUrgency = 'medium';
                    warningCount++;
                }
            }
        });

        return { worstUrgency, criticalCount, warningCount, totalZones: Object.keys(zones).length };
    },

    // ==========================================================================
    // API ENDPOINTS (simulating ESP32 web server)
    // ==========================================================================

    /**
     * POST /api/log_event - Log a physics event
     */
    logPhysicsEvent: async function (event) {
        // In real ESP32, this appends to /logs/physics_events.csv
        // Here we just store it
        if (!this.physicsEvents) this.physicsEvents = [];
        this.physicsEvents.push(event);
        return { success: true };
    },

    /**
     * GET /api/diagnostics - Get system diagnostics
     */
    getDiagnostics: function () {
        const uptime = Math.floor((Date.now() - (window.appStartTime || Date.now())) / 1000);
        const uptimeHours = uptime / 3600;

        // Get calibration state from physics engine
        let calibration = { status: 'Unknown', confidence: 0, events_captured: 0 };
        if (typeof Physics !== 'undefined' && Physics.autoCalibration) {
            const calState = Physics.autoCalibration.getCalibrationState();
            const conf = calState.confidence || 0;
            calibration = {
                status: conf < 0.35 ? 'Learning' : conf < 0.65 ? 'Calibrating' : 'Calibrated',
                confidence: conf,
                events_captured: calState.n_events || 0
            };
        }

        // Get latest sensor data
        const latest = this.db.length > 0 ? this.db[this.db.length - 1] : null;
        const lastReadingSecsAgo = latest ? Math.floor((Date.now() / 1000) - latest.timestamp) : 999;

        return {
            sd_card: {
                status: 'ok',
                free_gb: 31.9,
                last_write_seconds_ago: 2
            },
            sensors: {
                soil_status: 'ok',
                soil_last_raw: latest?.raw || 0,
                temp_status: 'ok',
                temp_last_c: latest?.temp_c || 0,
                failure_rate_percent: 0.2
            },
            system: {
                uptime_hours: Math.round(uptimeHours * 10) / 10,
                memory_free_kb: 145,
                last_reading_seconds_ago: lastReadingSecsAgo
            },
            calibration: calibration,
            errors_24h: 0
        };
    },

    /**
     * GET /api/config - Get user preferences
     */
    getConfig: function () {
        const stored = localStorage.getItem('agriscan_user_prefs');
        if (stored) {
            try {
                return App.normalizePrefs(JSON.parse(stored));
            } catch (e) {
                Logger.log('ERROR', 'Config parse error', e);
            }
        }
        // Return default config
        return {
            onboarding_complete: false,
            device_name: '',
            root_depth_cm: 30,
            crop: 'tomato',
            soil: 'loam',
            setup_date: null,
            planting_ts: null,
            farmer_name: '',
            notes: ''
        };
    },

    /**
     * POST /api/config - Save user preferences
     */
    saveConfig: function (config) {
        try {
            localStorage.setItem('agriscan_user_prefs', JSON.stringify(config));
            Logger.log('DATA', 'User config saved', config);
            return { success: true };
        } catch (e) {
            Logger.log('ERROR', 'Failed to save config', e);
            return { success: false, error: e.message };
        }
    },

    /**
     * GET /api/data?hours=24 - Get historical sensor data
     */
    getData: function (hours = 24) {
        const cutoff = Date.now() / 1000 - (hours * 3600);
        return this.db.filter(s => s.timestamp >= cutoff);
    }
};

// =============================================================================
// SIMULATION ENGINE
// =============================================================================

const Simulator = {
    running: false,
    currentSim: null,
    interval: null,
    logs: [],
    startTime: null,
    tickCount: 0,

    // Rain simulation - rapid wetting across zones
    simulateRain: function () {
        if (this.running) {
            Logger.log('SIMULATION', 'Stopping current simulation first...');
            this.stop();
        }

        // Enable simulation mode for faster calibration
        if (typeof Physics !== 'undefined' && Physics.enableSimulationMode) {
            Physics.enableSimulationMode();
        }

        Logger.log('SIMULATION', '🌧️ Starting RAIN simulation');
        this.running = true;
        this.currentSim = 'rain';
        this.logs = [];
        this.startTime = Date.now();
        this.tickCount = 0;

        const zones = Object.keys(MockAPI.zones);
        let ticksRemaining = 15;

        this.interval = setInterval(() => {
            this.tickCount++;
            ticksRemaining--;

            zones.forEach(zoneId => {
                let raw = MockAPI.zones[zoneId].currentRaw;
                // Rain decreases raw ADC (increases moisture)
                const rainIntensity = 80 + Math.random() * 60;
                raw -= rainIntensity;
                raw = Math.max(250, raw); // Don't go below saturation

                const sample = MockAPI.inject(raw, 20 + Math.random() * 2, zoneId);

                this.logs.push({
                    tick: this.tickCount,
                    time: Date.now() - this.startTime,
                    zone: zoneId,
                    raw: raw,
                    theta: sample?.theta,
                    status: sample?.status,
                    urgency: sample?.urgency
                });
            });

            Logger.log('SIMULATION', `Rain tick ${this.tickCount}/${15}`, {
                avgRaw: Math.round(zones.reduce((sum, z) => sum + MockAPI.zones[z].currentRaw, 0) / zones.length)
            });

            App.updateData();

            if (ticksRemaining <= 0) {
                this.stop();
                Logger.log('SIMULATION', '🌧️ Rain simulation complete', {
                    duration: Date.now() - this.startTime,
                    totalTicks: this.tickCount
                });
            }
        }, 300);
    },

    // Drought simulation - gradual drying
    simulateDrought: function () {
        if (this.running) {
            Logger.log('SIMULATION', 'Stopping current simulation first...');
            this.stop();
        }

        // Enable simulation mode for faster calibration
        if (typeof Physics !== 'undefined' && Physics.enableSimulationMode) {
            Physics.enableSimulationMode();
        }

        Logger.log('SIMULATION', '☀️ Starting DROUGHT simulation');
        this.running = true;
        this.currentSim = 'drought';
        this.logs = [];
        this.startTime = Date.now();
        this.tickCount = 0;

        const zones = Object.keys(MockAPI.zones);
        let ticksRemaining = 20;

        this.interval = setInterval(() => {
            this.tickCount++;
            ticksRemaining--;

            zones.forEach(zoneId => {
                let raw = MockAPI.zones[zoneId].currentRaw;
                // Drought increases raw ADC (decreases moisture)
                const dryingRate = 20 + Math.random() * 30;
                raw += dryingRate;
                raw = Math.min(950, raw); // Don't exceed dry limit

                const sample = MockAPI.inject(raw, 28 + Math.random() * 4, zoneId);

                this.logs.push({
                    tick: this.tickCount,
                    time: Date.now() - this.startTime,
                    zone: zoneId,
                    raw: raw,
                    theta: sample?.theta,
                    status: sample?.status,
                    urgency: sample?.urgency
                });
            });

            Logger.log('SIMULATION', `Drought tick ${this.tickCount}/${20}`, {
                avgRaw: Math.round(zones.reduce((sum, z) => sum + MockAPI.zones[z].currentRaw, 0) / zones.length)
            });

            App.updateData();

            if (ticksRemaining <= 0) {
                this.stop();
                Logger.log('SIMULATION', '☀️ Drought simulation complete', {
                    duration: Date.now() - this.startTime,
                    totalTicks: this.tickCount
                });
            }
        }, 400);
    },

    // 10-minute long run simulation with realistic patterns
    simulateLongRun: function () {
        if (this.running) {
            Logger.log('SIMULATION', 'Stopping current simulation first...');
            this.stop();
        }

        // Enable simulation mode for faster calibration
        if (typeof Physics !== 'undefined' && Physics.enableSimulationMode) {
            Physics.enableSimulationMode();
        }

        const DURATION_MS = 10 * 60 * 1000; // 10 minutes
        const TICK_INTERVAL = 2000; // Every 2 seconds
        const TOTAL_TICKS = DURATION_MS / TICK_INTERVAL;

        Logger.log('SIMULATION', '⏱️ Starting 10-MINUTE LONG RUN simulation', {
            duration: '10 minutes',
            tickInterval: '2 seconds',
            totalTicks: TOTAL_TICKS
        });

        this.running = true;
        this.currentSim = 'longrun';
        this.logs = [];
        this.startTime = Date.now();
        this.tickCount = 0;

        const zones = Object.keys(MockAPI.zones);

        // Simulation phases
        const phases = [
            { name: 'baseline', start: 0, end: 0.1, trend: 'stable' },
            { name: 'morning_drying', start: 0.1, end: 0.3, trend: 'drying' },
            { name: 'irrigation_event', start: 0.3, end: 0.35, trend: 'wetting' },
            { name: 'drainage', start: 0.35, end: 0.5, trend: 'draining' },
            { name: 'afternoon_drying', start: 0.5, end: 0.7, trend: 'drying' },
            { name: 'evening_stable', start: 0.7, end: 0.85, trend: 'stable' },
            { name: 'night_recovery', start: 0.85, end: 1.0, trend: 'slight_wetting' }
        ];

        this.interval = setInterval(() => {
            this.tickCount++;
            const progress = this.tickCount / TOTAL_TICKS;
            const elapsedMin = ((Date.now() - this.startTime) / 60000).toFixed(1);

            // Determine current phase
            const currentPhase = phases.find(p => progress >= p.start && progress < p.end) || phases[phases.length - 1];

            zones.forEach(zoneId => {
                let raw = MockAPI.zones[zoneId].currentRaw;
                let temp = 22;

                // Apply phase-specific behavior
                switch (currentPhase.trend) {
                    case 'stable':
                        raw += (Math.random() - 0.5) * 10;
                        temp = 22 + Math.random() * 2;
                        break;
                    case 'drying':
                        raw += 5 + Math.random() * 15;
                        temp = 26 + Math.random() * 4;
                        break;
                    case 'wetting':
                        raw -= 40 + Math.random() * 30;
                        temp = 20 + Math.random() * 2;
                        break;
                    case 'draining':
                        raw += 2 + Math.random() * 8;
                        temp = 24 + Math.random() * 2;
                        break;
                    case 'slight_wetting':
                        raw -= 2 + Math.random() * 5;
                        temp = 18 + Math.random() * 2;
                        break;
                }

                raw = Math.max(250, Math.min(950, raw));

                const sample = MockAPI.inject(raw, temp, zoneId);

                this.logs.push({
                    tick: this.tickCount,
                    time: Date.now() - this.startTime,
                    elapsed_min: parseFloat(elapsedMin),
                    phase: currentPhase.name,
                    zone: zoneId,
                    raw: Math.round(raw),
                    temp: temp.toFixed(1),
                    theta: sample?.theta,
                    theta_pct: sample?.theta ? (sample.theta * 100).toFixed(2) : null,
                    psi_kPa: sample?.psi_kPa?.toFixed(2),
                    AW_mm: sample?.AW_mm?.toFixed(2),
                    status: sample?.status,
                    urgency: sample?.urgency,
                    confidence: sample?.confidence?.toFixed(3),
                    regime: sample?.regime
                });
            });

            // Log progress every 30 seconds
            if (this.tickCount % 15 === 0) {
                const globalStatus = MockAPI.getGlobalStatus();
                Logger.log('SIMULATION', `Long run: ${elapsedMin} min (${(progress * 100).toFixed(0)}%) - Phase: ${currentPhase.name}`, {
                    worstUrgency: globalStatus.worstUrgency,
                    critical: globalStatus.criticalCount,
                    warning: globalStatus.warningCount
                });
            }

            App.updateData();

            if (this.tickCount >= TOTAL_TICKS) {
                this.stop();
                this.generateReport();
            }
        }, TICK_INTERVAL);

        // Show countdown in UI
        this.updateCountdown(DURATION_MS);
    },

    updateCountdown: function (remainingMs) {
        const countdownEl = document.getElementById('sim-countdown');
        if (countdownEl && this.running) {
            const elapsed = Date.now() - this.startTime;
            const remaining = Math.max(0, remainingMs - elapsed);
            const mins = Math.floor(remaining / 60000);
            const secs = Math.floor((remaining % 60000) / 1000);
            countdownEl.textContent = `${mins}:${secs.toString().padStart(2, '0')} remaining`;

            if (remaining > 0) {
                setTimeout(() => this.updateCountdown(remainingMs), 1000);
            }
        }
    },

    stop: function () {
        if (this.interval) {
            clearInterval(this.interval);
            this.interval = null;
        }
        this.running = false;

        // Disable simulation mode
        if (typeof Physics !== 'undefined' && Physics.disableSimulationMode) {
            Physics.disableSimulationMode();
        }

        Logger.log('SIMULATION', `Simulation "${this.currentSim}" stopped`, {
            totalTicks: this.tickCount,
            totalLogs: this.logs.length
        });
    },

    generateReport: function () {
        Logger.log('SIMULATION', '📊 Generating simulation report...');

        const report = {
            simulation: this.currentSim,
            startTime: new Date(this.startTime).toISOString(),
            endTime: new Date().toISOString(),
            duration_ms: Date.now() - this.startTime,
            duration_readable: `${((Date.now() - this.startTime) / 60000).toFixed(1)} minutes`,
            totalTicks: this.tickCount,
            totalDataPoints: this.logs.length,
            zones: Object.keys(MockAPI.zones),
            summary: this.calculateSummary()
        };

        Logger.log('SIMULATION', '📊 Report complete', report.summary);

        console.log('%c=== SIMULATION REPORT ===', 'font-size: 16px; font-weight: bold; color: #1976D2;');
        console.table(report.summary.byZone);

        return report;
    },

    calculateSummary: function () {
        const byZone = {};
        const zones = Object.keys(MockAPI.zones);

        zones.forEach(zoneId => {
            const zoneLogs = this.logs.filter(l => l.zone === zoneId);
            const thetaValues = zoneLogs.map(l => l.theta).filter(v => v !== null);

            byZone[zoneId] = {
                dataPoints: zoneLogs.length,
                theta_min: (Math.min(...thetaValues) * 100).toFixed(2) + '%',
                theta_max: (Math.max(...thetaValues) * 100).toFixed(2) + '%',
                theta_avg: ((thetaValues.reduce((a, b) => a + b, 0) / thetaValues.length) * 100).toFixed(2) + '%',
                final_status: zoneLogs[zoneLogs.length - 1]?.status,
                final_urgency: zoneLogs[zoneLogs.length - 1]?.urgency
            };
        });

        return {
            byZone,
            totalDataPoints: this.logs.length,
            phasesExecuted: [...new Set(this.logs.map(l => l.phase))].filter(Boolean)
        };
    },

    exportLogs: function () {
        return this.logs;
    }
};

// =============================================================================
// MAIN APP CONTROLLER
// =============================================================================

const App = {
    state: {
        currentTier: 1,
        currentView: 'home',
        currentSample: null,
        selectedZone: null,  // No zone selected by default
        history: [],
        settings: {
            tempUnit: 'c',
            threshCritical: 30,
            threshWarning: 50,
            crop: 'tomato',
            soil: 'loam',
            plantingDate: null,
            planting_ts: null
        }
    },

    init: async function () {
        Logger.log('UI', 'Initializing AgriScan App...');

        // Initialize I18n
        if (typeof I18n !== 'undefined') {
            I18n.init();
        }

        // Initialize MockAPI
        MockAPI.init();

        await this.loadThresholdsData();
        this.loadSettings();

        // Set default planting date
        if (!this.state.settings.plantingDate) {
            const d = new Date();
            d.setDate(d.getDate() - 45);
            this.state.settings.plantingDate = d.toISOString().split('T')[0];
        }
        if (!this.state.settings.planting_ts && this.state.settings.plantingDate) {
            this.state.settings.planting_ts = Math.floor(new Date(this.state.settings.plantingDate).getTime() / 1000);
        }

        this.applyThresholdConfig();
        this.persistCanonicalSettings();

        // Seed initial data
        MockAPI.seed();

        this.setupListeners();
        this.renderZoneGrid();
        this.syncBottomNav('home');
        this.updateData();

        // Polling loop
        setInterval(() => this.updateData(), 3000);

        Logger.log('UI', 'App initialization complete', {
            zones: Object.keys(MockAPI.zones).length,
            samples: MockAPI.db.length
        });
    },

    updateData: async function () {
        const sample = await MockAPI.getCurrent(this.state.selectedZone);
        if (sample) {
            this.state.currentSample = sample;
            this.state.history = await MockAPI.getSeries(this.state.selectedZone);
            this.render();
        }
    },

    // --- NAVIGATION ---
    navigate: function (view) {
        const routeToSection = {
            home: 'tier-1',
            homeInsights: 'tier-2',
            history: 'tier-3',
            map: 'tier-map',
            settings: 'settings'
        };
        const sectionId = routeToSection[view] || 'tier-1';
        this.state.currentView = view;
        this.showSection(sectionId);
        this.syncBottomNav(view);
    },

    showSection: function (sectionId) {
        document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
        const section = document.getElementById(sectionId);
        if (section) section.classList.add('active');
        Logger.log('UI', `Navigated to ${sectionId}`);

        if (sectionId === 'tier-3') this.renderTier3();
        if (sectionId === 'tier-map') this.renderZoneGrid();
    },

    syncBottomNav: function (view) {
        const activeByView = {
            home: 'nav-home',
            homeInsights: 'nav-home',
            history: 'nav-history',
            map: 'nav-map',
            settings: 'nav-settings'
        };
        document.querySelectorAll('.nav-item').forEach((el) => el.classList.remove('active'));
        const activeId = activeByView[view];
        if (activeId) {
            const activeEl = document.getElementById(activeId);
            if (activeEl) activeEl.classList.add('active');
        }
    },

    goToTier: function (tier) {
        if (tier === 1) return this.navigate('home');
        if (tier === 2) return this.navigate('homeInsights');
        if (tier === 3) return this.navigate('history');
        if (tier === 'map') return this.navigate('map');
        this.navigate('home');
    },

    goToSettings: function () {
        this.navigate('settings');

        const s = this.state.settings;
        if (document.getElementById('set-crop')) document.getElementById('set-crop').value = s.crop || 'tomato';
        if (document.getElementById('set-soil')) document.getElementById('set-soil').value = s.soil || 'loam';
        if (document.getElementById('set-date')) document.getElementById('set-date').value = s.plantingDate;
        if (document.getElementById('set-flow')) document.getElementById('set-flow').value = s.flowRate || 1000;
        if (document.getElementById('set-lang')) document.getElementById('set-lang').value = I18n?.getLang() || 'en';

        // Auto-refresh live sensor data
        this.refreshSensorData();
    },

    saveSettings: function () {
        const s = this.state.settings;

        const cropEl = document.getElementById('set-crop');
        if (cropEl) {
            s.crop = cropEl.value;
        }

        const soilEl = document.getElementById('set-soil');
        if (soilEl) {
            s.soil = soilEl.value;
        }

        const dateEl = document.getElementById('set-date');
        if (dateEl) s.plantingDate = dateEl.value;
        if (s.plantingDate) s.planting_ts = Math.floor(new Date(s.plantingDate).getTime() / 1000);

        const flowEl = document.getElementById('set-flow');
        if (flowEl) s.flowRate = parseFloat(flowEl.value);

        const critEl = document.getElementById('set-crit-slide');
        if (critEl) s.threshCritical = parseInt(critEl.value);

        this.applyThresholdConfig();
        this.persistCanonicalSettings();
        Logger.log('UI', 'Settings saved', s);
        this.updateData();
        alert("Configuration Saved!");
        this.goToTier(3);
    },

    loadSettings: function () {
        try {
            const canonicalRaw = localStorage.getItem('agriscan_user_prefs');
            const legacyRaw = localStorage.getItem('agriscan_settings');

            const canonical = canonicalRaw ? this.normalizePrefs(JSON.parse(canonicalRaw)) : {};
            const legacy = legacyRaw ? this.normalizePrefs(JSON.parse(legacyRaw)) : {};
            const merged = { ...legacy, ...canonical };

            this.state.settings = {
                ...this.state.settings,
                crop: merged.crop || this.state.settings.crop,
                soil: merged.soil || this.state.settings.soil,
                plantingDate: merged.plantingDate || this.state.settings.plantingDate,
                planting_ts: merged.planting_ts || this.state.settings.planting_ts,
                flowRate: merged.flowRate ?? this.state.settings.flowRate
            };
            Logger.log('UI', 'Settings loaded from localStorage');
        } catch (e) {
            Logger.log('ERROR', 'Settings parse error', e);
        }
    },

    normalizePrefs: function (raw) {
        const setupTs = typeof raw.setup_date === 'string'
            ? Math.floor(new Date(raw.setup_date).getTime() / 1000)
            : (raw.setup_date || null);

        const plantingTs = raw.planting_ts
            || (raw.plantingDate ? Math.floor(new Date(raw.plantingDate).getTime() / 1000) : null)
            || setupTs;

        const plantingDate = raw.plantingDate
            || (plantingTs ? new Date(plantingTs * 1000).toISOString().split('T')[0] : null);

        return {
            onboarding_complete: Boolean(raw.onboarding_complete),
            crop: raw.crop || raw.crop_type || null,
            soil: raw.soil || null,
            setup_date: setupTs,
            planting_ts: plantingTs,
            plantingDate,
            flowRate: raw.flowRate
        };
    },

    persistCanonicalSettings: function () {
        const s = this.state.settings;
        const setupDate = Math.floor(Date.now() / 1000);
        const payload = {
            onboarding_complete: true,
            device_name: 'AgriScan Sensor',
            root_depth_cm: 30,
            crop: s.crop,
            soil: s.soil,
            setup_date: setupDate,
            planting_ts: s.planting_ts || setupDate,
            farmer_name: '',
            notes: '',
            plantingDate: s.plantingDate,
            flowRate: s.flowRate
        };
        localStorage.setItem('agriscan_user_prefs', JSON.stringify(payload));
        localStorage.removeItem('agriscan_settings');
    },

    loadThresholdsData: async function () {
        try {
            await THRESHOLDS_STORE.load();
            this.populateThresholdSelectors();
        } catch (e) {
            Logger.log('ERROR', 'Failed to load thresholds data', e);
        }
    },

    populateThresholdSelectors: function () {
        const cropEl = document.getElementById('set-crop');
        const soilEl = document.getElementById('set-soil');
        const data = THRESHOLDS_STORE.data;
        if (!data) return;

        if (cropEl) {
            cropEl.innerHTML = '';
            Object.entries(data.crops || {}).forEach(([key, crop]) => {
                const option = document.createElement('option');
                option.value = key;
                option.textContent = crop.display_name || key;
                cropEl.appendChild(option);
            });
        }

        if (soilEl) {
            soilEl.innerHTML = '';
            Object.entries(data.soils || {}).forEach(([key, soil]) => {
                const option = document.createElement('option');
                option.value = key;
                option.textContent = soil.label || key;
                soilEl.appendChild(option);
            });
        }
    },

    applyThresholdConfig: function () {
        const s = this.state.settings;
        const thresholdConfig = THRESHOLDS_STORE.getThresholds(s.crop, s.soil, s.planting_ts);
        if (!thresholdConfig) return;

        s.threshCritical = Math.round(thresholdConfig.theta_refill * 100);
        s.threshWarning = Math.round(thresholdConfig.theta_fc * 100);

        if (typeof Physics !== 'undefined' && Physics.configureCropSoil) {
            Physics.configureCropSoil({
                crop: s.crop,
                soil: s.soil,
                planting_ts: s.planting_ts,
                ...thresholdConfig
            });
        }
    },

    goToMap: function () {
        this.navigate('map');
    },

    getDaysAfterPlanting: function () {
        if (!this.state.settings.plantingDate) return 0;
        const start = new Date(this.state.settings.plantingDate);
        const now = new Date();
        return Math.floor((now - start) / (1000 * 60 * 60 * 24));
    },

    setLang: function (code) {
        if (typeof I18n !== 'undefined' && I18n.setLang(code)) {
            Logger.log('I18N', `Language changed to ${code}`);
            this.render();
        }
    },

    // --- RENDERING ---
    render: function () {
        if (!this.state.currentSample) return;
        this.renderTier1();
        this.renderTier2();
        const historySection = document.getElementById('tier-3');
        if (historySection && historySection.classList.contains('active')) this.renderTier3();

        // Only update zone UI if map section exists and is active
        const mapSection = document.getElementById('tier-map');
        if (mapSection && mapSection.classList.contains('active')) {
            this.updateZoneGridColors();
            this.renderZoneDetails();
        }
    },

    renderTier1: function () {
        const s = this.state.currentSample;
        const greetingEl = document.getElementById('t1-greeting');
        const statusIcon = document.getElementById('t1-status-icon');
        const msgEl = document.getElementById('t1-message');
        const instrEl = document.getElementById('t1-instruction');
        const timeEl = document.getElementById('t1-time');
        const moistureEl = document.getElementById('t1-glance-moisture');
        const tempEl = document.getElementById('t1-glance-temp');
        const zonesEl = document.getElementById('t1-glance-zones');

        if (!statusIcon || !msgEl) return;

        statusIcon.className = 'status-circle-large';
        const hour = new Date().getHours();
        let greeting = 'Good evening';
        if (hour < 12) greeting = 'Good morning';
        else if (hour < 17) greeting = 'Good afternoon';
        if (greetingEl) greetingEl.textContent = `${greeting} 👋`;

        if (s.urgency === 'high') {
            statusIcon.classList.add('critical');
            msgEl.textContent = 'Your field needs watering attention today.';
            instrEl.textContent = 'Moisture is below target in at least one zone. Plan irrigation now.';
        } else if (s.urgency === 'medium') {
            statusIcon.classList.add('warning');
            msgEl.textContent = 'Your field is stable, but a few areas are drying.';
            instrEl.textContent = 'No emergency yet. Check again later today.';
        } else {
            statusIcon.classList.add('healthy');
            msgEl.textContent = 'Your field is looking healthy today.';
            instrEl.textContent = 'Soil moisture is at good levels across your active zones.';
        }

        if (timeEl) {
            timeEl.textContent = new Date(s.timestamp * 1000).toLocaleTimeString();
        }
        if (moistureEl) moistureEl.textContent = s.status || 'Optimal';
        if (tempEl) tempEl.textContent = `${s.temp_c.toFixed(1)}°C`;
        if (zonesEl) zonesEl.textContent = `${Object.keys(MockAPI.zones || {}).length} sensors`;
    },

    renderTier2: function () {
        const s = this.state.currentSample;
        const list = document.getElementById('t2-reasons');
        const titleEl = document.querySelector('#tier-2 .view-header h2');

        if (!list) return;
        list.innerHTML = '';

        // Update title based on status
        if (titleEl) {
            if (s.urgency === 'high') {
                titleEl.textContent = I18n?.t('why_title_irrigate') || 'Why do I need to water?';
            } else if (s.urgency === 'medium') {
                titleEl.textContent = I18n?.t('why_title_check') || 'Why should I check the field?';
            } else {
                titleEl.textContent = I18n?.t('why_title_good') || 'Why is everything good?';
            }
        }

        // VWC Status
        const li1 = document.createElement('li');
        li1.innerHTML = `<strong>VWC is ${(s.theta * 100).toFixed(1)}%</strong> (FC: ${(s.theta_fc * 100).toFixed(0)}%)`;
        list.appendChild(li1);

        // Yield Risk
        if (s.urgency === 'high') {
            const liYield = document.createElement('li');
            liYield.style.color = '#C62828';
            const cropMeta = THRESHOLDS_STORE.getCrop(this.state.settings.crop);
            const stageMeta = THRESHOLDS_STORE.getCropStage(this.state.settings.crop, this.state.settings.planting_ts);
            const cropLabel = cropMeta?.display_name || this.state.settings.crop || "Unknown";
            const stageLabel = stageMeta?.name || "current stage";
            liYield.innerHTML = `⚠️ <strong>Yield Risk: Elevated for ${cropLabel} (${stageLabel})</strong>`;
            list.appendChild(liYield);
        }

        // Crop Age
        const liStage = document.createElement('li');
        liStage.innerHTML = `Crop Age: <strong>${this.getDaysAfterPlanting()} days</strong>`;
        list.appendChild(liStage);

        // Regime
        const li2 = document.createElement('li');
        li2.textContent = `Regime: ${s.regime?.toUpperCase() || 'UNKNOWN'}`;
        list.appendChild(li2);

        // Confidence
        const liConf = document.createElement('li');
        liConf.innerHTML = `Calibration Confidence: <strong>${(s.confidence * 100).toFixed(0)}%</strong>`;
        list.appendChild(liConf);
    },

    renderTier3: function () {
        const s = this.state.currentSample;

        this.setSafeText('t3-moist', (s.theta * 100).toFixed(1) + '%');
        this.setSafeText('t3-soil-temp', s.temp_c.toFixed(1) + '°C');
        this.setSafeText('t3-vpd', s.psi_kPa ? s.psi_kPa.toFixed(1) + ' kPa' : '--');
        this.setSafeText('t3-ec', s.raw || '--');
        this.setSafeText('t3-leaf', s.AW_mm ? s.AW_mm.toFixed(1) + ' mm' : '--');
        this.setSafeText('t3-depletion', s.fractionDepleted ? (s.fractionDepleted * 100).toFixed(0) + '%' : '--');

        const mCell = document.getElementById('t3-moist-cell');
        if (mCell) {
            mCell.className = 'metric-cell';
            if (s.urgency === 'high') mCell.classList.add('critical');
            else if (s.urgency === 'medium') mCell.classList.add('warning');
            else mCell.classList.add('healthy');
        }

        this.renderChart();
    },

    renderChart: function () {
        const container = document.getElementById('tier3-chart');
        if (!container) return;

        const history = this.state.history;
        if (history.length < 2) return;

        const W = container.clientWidth || 300;
        const H = container.clientHeight || 200;
        const pad = 25;

        const tMin = history[0].timestamp;
        const tMax = history[history.length - 1].timestamp;
        const vMin = 0, vMax = 0.5;

        const getX = t => pad + ((t - tMin) / (tMax - tMin + 1)) * (W - 2 * pad);
        const getY = v => H - pad - ((v - vMin) / (vMax - vMin)) * (H - 2 * pad);

        let d = `M ${getX(history[0].timestamp)} ${getY(history[0].theta)}`;
        let markers = '';

        for (let i = 1; i < history.length; i++) {
            d += ` L ${getX(history[i].timestamp)} ${getY(history[i].theta)}`;
            if ((history[i].theta - history[i - 1].theta) > 0.02) {
                const x = getX(history[i].timestamp);
                markers += `<line x1="${x}" y1="${pad}" x2="${x}" y2="${H - pad}" stroke="#2196F3" stroke-width="1" stroke-dasharray="2" />`;
            }
        }

        container.innerHTML = `
            <svg width="100%" height="100%" viewBox="0 0 ${W} ${H}">
                <rect x="${pad}" y="${getY(0.35)}" width="${W - 2 * pad}" height="${getY(0.25) - getY(0.35)}" fill="rgba(46,125,50,0.1)" />
                <line x1="${pad}" y1="${getY(0.32)}" x2="${W - pad}" y2="${getY(0.32)}" stroke="#2E7D32" stroke-width="1" stroke-dasharray="4" />
                <text x="${W - pad - 5}" y="${getY(0.32) - 3}" text-anchor="end" font-size="10" fill="#2E7D32">FC</text>
                <line x1="${pad}" y1="${getY(0.12)}" x2="${W - pad}" y2="${getY(0.12)}" stroke="#C62828" stroke-width="1" stroke-dasharray="4" />
                <text x="${W - pad - 5}" y="${getY(0.12) - 3}" text-anchor="end" font-size="10" fill="#C62828">WP</text>
                ${markers}
                <path d="${d}" fill="none" stroke="#2E7D32" stroke-width="2" />
                <line x1="${pad}" y1="${H - pad}" x2="${W - pad}" y2="${H - pad}" stroke="#ccc" />
                <line x1="${pad}" y1="${pad}" x2="${pad}" y2="${H - pad}" stroke="#ccc" />
                <text x="${pad}" y="${H - 5}" font-size="10" fill="#666">Start</text>
                <text x="${W - pad}" y="${H - 5}" text-anchor="end" font-size="10" fill="#666">Now</text>
            </svg>
        `;
    },

    // --- ZONE GRID ---
    renderZoneGrid: function () {
        const container = document.getElementById('zone-grid');
        if (!container) return;

        container.innerHTML = '';

        ZONE_CONFIG.sensors.forEach(sensor => {
            const cell = document.createElement('div');
            cell.className = 'zone-cell';
            cell.id = `zone-${sensor.id}`;
            cell.textContent = sensor.id;

            if (!sensor.active) {
                cell.classList.add('inactive');
            } else {
                cell.classList.add('healthy');
                cell.onclick = () => this.selectZone(sensor.id);
            }

            container.appendChild(cell);
        });

        this.updateZoneGridColors();
        Logger.log('ZONE', 'Zone grid rendered');
    },

    updateZoneGridColors: function () {
        const zones = MockAPI.getAllZones();

        Object.keys(zones).forEach(zoneId => {
            const cell = document.getElementById(`zone-${zoneId}`);
            if (cell && zones[zoneId].latest) {
                cell.className = 'zone-cell';
                const urgency = zones[zoneId].latest.urgency;

                if (urgency === 'high') cell.classList.add('critical');
                else if (urgency === 'medium') cell.classList.add('warning');
                else cell.classList.add('healthy');

                if (zoneId === this.state.selectedZone) {
                    cell.classList.add('selected');
                }
            }
        });
    },

    renderZoneDetails: function () {
        const detailsEl = document.getElementById('zone-details');
        if (!detailsEl) return;

        if (!this.state.selectedZone) {
            if (detailsEl.style.display !== 'none') {
                detailsEl.style.display = 'none';
            }
            return;
        }

        const zones = MockAPI.getAllZones();
        const zone = zones[this.state.selectedZone];
        if (!zone || !zone.latest) {
            detailsEl.style.display = 'none';
            return;
        }

        const s = zone.latest;
        detailsEl.style.display = 'block';

        // Update zone name
        const nameEl = document.getElementById('zone-name');
        if (nameEl) nameEl.textContent = this.state.selectedZone;

        // Update metrics
        const vwcEl = document.getElementById('zone-vwc');
        if (vwcEl) vwcEl.textContent = `${(s.theta * 100).toFixed(1)}%`;

        const psiEl = document.getElementById('zone-psi');
        if (psiEl) psiEl.textContent = `${s.psi_kPa?.toFixed(1) || '--'} kPa`;

        const awEl = document.getElementById('zone-aw');
        if (awEl) awEl.textContent = `${s.AW_mm?.toFixed(1) || '--'} mm`;

        const deplEl = document.getElementById('zone-depl');
        if (deplEl) deplEl.textContent = `${(s.fractionDepleted * 100).toFixed(0)}%`;

        const statusEl = document.getElementById('zone-status');
        if (statusEl) statusEl.textContent = s.status || '--';

        const regimeEl = document.getElementById('zone-regime');
        if (regimeEl) regimeEl.textContent = s.regime || '--';

        // Only log on initial selection, not every update
        // Logger is called from selectZone() instead
    },

    selectZone: function (zoneId) {
        this.state.selectedZone = zoneId;
        Logger.log('ZONE', `Selected zone: ${zoneId}`);
        this.updateData();
        this.updateZoneGridColors();
    },

    clearZoneSelection: function () {
        this.state.selectedZone = null;
        const detailsEl = document.getElementById('zone-details');
        if (detailsEl) detailsEl.style.display = 'none';
        this.updateZoneGridColors();
        Logger.log('ZONE', 'Cleared zone selection');
    },

    // --- SIMULATIONS ---
    simulateRain: function () {
        Simulator.simulateRain();
    },

    simulateDrought: function () {
        Simulator.simulateDrought();
    },

    simulateLongRun: function () {
        Simulator.simulateLongRun();
    },

    stopSimulation: function () {
        Simulator.stop();
    },

    // --- EXPORT ---
    exportCSV: function () {
        let csv = "Timestamp,Zone,Raw,Temp,Theta,Theta_Pct,Psi_kPa,AW_mm,Depletion,Status,Urgency,Regime,Confidence\n";

        this.state.history.forEach(h => {
            csv += `${h.timestamp},${h.zoneId || 'A1'},${h.raw},${h.temp_c?.toFixed(1)},`;
            csv += `${h.theta?.toFixed(4)},${(h.theta * 100).toFixed(2)},${h.psi_kPa?.toFixed(2)},`;
            csv += `${h.AW_mm?.toFixed(2)},${h.fractionDepleted?.toFixed(3)},${h.status},`;
            csv += `${h.urgency},${h.regime},${h.confidence?.toFixed(3)}\n`;
        });

        this.downloadFile(csv, `agriscan_data_${Date.now()}.csv`, 'text/csv');
        Logger.log('DATA', `Exported ${this.state.history.length} records to CSV`);
    },

    exportSimulationLogs: function () {
        const logs = Simulator.exportLogs();
        const json = JSON.stringify(logs, null, 2);
        this.downloadFile(json, `simulation_logs_${Date.now()}.json`, 'application/json');
        Logger.log('DATA', `Exported ${logs.length} simulation log entries`);
    },

    exportAllData: function () {
        const data = {
            exportTime: new Date().toISOString(),
            settings: this.state.settings,
            zones: MockAPI.getAllZones(),
            allSamples: MockAPI.db,
            simulationLogs: Simulator.logs,
            systemLogs: Logger.history
        };

        const json = JSON.stringify(data, null, 2);
        this.downloadFile(json, `agriscan_full_export_${Date.now()}.json`, 'application/json');
        Logger.log('DATA', 'Full data export complete');
    },

    downloadFile: function (content, filename, type) {
        const blob = new Blob([content], { type });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);
    },

    // --- NAVIGATION (Diagnostics) ---
    goToDiagnostics: function () {
        window.location.href = 'diagnostics.html';
    },

    // --- LIVE SENSOR DATA OUTPUT ---
    refreshSensorData: function () {
        const sample = this.state.currentSample;

        if (sample) {
            // Update raw ADC value
            const rawEl = document.getElementById('live-soil-raw');
            if (rawEl) rawEl.textContent = sample.raw || '--';

            // Update VWC percentage
            const vwcEl = document.getElementById('live-soil-vwc');
            if (vwcEl) vwcEl.textContent = sample.theta ? (sample.theta * 100).toFixed(1) + '%' : '--%';

            // Update temperature
            const tempEl = document.getElementById('live-temp');
            if (tempEl) {
                const temp = sample.temp_c || 0;
                if (this.state.settings.tempUnit === 'f') {
                    tempEl.textContent = ((temp * 9 / 5) + 32).toFixed(1) + '°F';
                } else {
                    tempEl.textContent = temp.toFixed(1) + '°C';
                }
            }

            // Update timestamp
            const tsEl = document.getElementById('live-timestamp');
            if (tsEl) {
                const date = new Date(sample.timestamp * 1000);
                tsEl.textContent = date.toLocaleTimeString();
            }

            Logger.log('UI', 'Sensor data refreshed', {
                raw: sample.raw,
                theta: sample.theta,
                temp: sample.temp_c
            });
        } else {
            Logger.log('UI', 'No sensor data available');
        }
    },

    // --- UTILITIES ---
    setSafeText: function (id, text) {
        const el = document.getElementById(id);
        if (el) el.textContent = text;
    },

    setupListeners: function () {
        const btnC = document.getElementById('btn-c');
        const btnF = document.getElementById('btn-f');
        if (btnC && btnF) {
            btnC.addEventListener('click', () => {
                this.state.settings.tempUnit = 'c';
                btnC.classList.add('active');
                btnF.classList.remove('active');
                this.render();
            });
            btnF.addEventListener('click', () => {
                this.state.settings.tempUnit = 'f';
                btnF.classList.add('active');
                btnC.classList.remove('active');
                this.render();
            });
        }
    }
};

// =============================================================================
// GLOBAL EXPORTS
// =============================================================================

// Track app start time for uptime calculations
window.appStartTime = Date.now();

window.App = App;
window.Simulator = Simulator;
window.Logger = Logger;
window.MockAPI = MockAPI;
window.PhysicsEventLogger = PhysicsEventLogger;

document.addEventListener('DOMContentLoaded', () => App.init());
