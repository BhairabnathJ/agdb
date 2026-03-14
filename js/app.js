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

// Zone map is built dynamically from paired CropBand devices (Issue 8)
// ZONE_CONFIG and ZONE_LABELS removed — zones come from buildZoneMapFromDevices()

async function buildZoneMapFromDevices() {
    if (window.HARDWARE_MODE) {
        try {
            const res = await fetch('/api/devices');
            if (!res.ok) throw new Error('devices API error');
            const devices = await res.json();
            const paired = devices.filter(d => d.paired === true);
            if (paired.length === 0) return { empty: true, message: 'No CropBands paired yet.' };
            return paired.map((d, i) => ({
                id: d.mac || d.id || ('DEV' + i),
                name: d.name || ('Zone ' + (i + 1)),
                row: Math.floor(i / 4),
                col: i % 4,
                active: true,
                battery: d.battery ?? 100,
                lastSeen: d.last_seen || null
            }));
        } catch (e) {
            Logger.log('ERROR', 'buildZoneMapFromDevices failed', e);
            return { empty: true, message: 'Could not reach device API.' };
        }
    }
}

// Standalone helper: hours until theta drops to theta_refill (Issue 4)
function calcTimeToCritical(sample) {
    if (!sample) return null;
    const dr = sample.drying_rate || sample.dryingRate_per_hr || 0;
    const theta = sample.theta;
    const refill = sample.theta_refill;
    if (!dr || dr <= 0 || theta == null || refill == null) return null;
    const delta = theta - refill;
    if (delta <= 0) return 0;
    return delta / dr; // hours
}

// Irrigation duration helper (Issue 10)
function calcIrrigationDuration(Dr_mm, flowRate_lpm) {
    if (!Dr_mm || Dr_mm <= 0) return { text: 'No deficit', precise: null };
    if (flowRate_lpm && flowRate_lpm > 0) {
        const minutes = Dr_mm / flowRate_lpm * 1000; // Dr_mm * area / flow — simplified per mm
        return {
            text: `~${Math.ceil(minutes)} min`,
            precise: minutes
        };
    }
    // No flow rate — return range using 5–15 L/min typical
    const minMins = Math.ceil(Dr_mm / 15 * 1000);
    const maxMins = Math.ceil(Dr_mm / 5 * 1000);
    return { text: `${minMins}–${maxMins} min (typical)`, precise: null };
}

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

        // Physics event logged locally only; ESP32 records events to its own CSV


        return event;
    },

    /**
     * Check for physics events by comparing current state to previous
     * Called after each sensor reading
     */
    checkForEvents: function (sample) {
        if (!sample || typeof window.Physics === 'undefined') return;

        const calState = window.Physics?.autoCalibration?.getCalibrationState();
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
// REAL HARDWARE API (Issue 6)
// =============================================================================

const RealAPI = {
    getLatest: async function (zoneId = null) {
        const url = zoneId ? `/api/current?zone=${encodeURIComponent(zoneId)}` : '/api/current';
        const res = await fetch(url);
        if (!res.ok) throw new Error('RealAPI.getLatest failed');
        return res.json();
    },

    getAllZones: async function () {
        const res = await fetch('/api/devices');
        if (!res.ok) throw new Error('RealAPI.getAllZones failed');
        return res.json();
    },

    getSeries: async function (zoneId, limit = 144) {
        const url = zoneId
            ? `/api/series?zone=${encodeURIComponent(zoneId)}&limit=${limit}`
            : `/api/series?limit=${limit}`;
        const res = await fetch(url);
        if (!res.ok) throw new Error('RealAPI.getSeries failed');
        return res.json();
    },

    getDiagnostics: async function () {
        const res = await fetch('/api/diagnostics');
        if (!res.ok) throw new Error('RealAPI.getDiagnostics failed');
        return res.json();
    }
};



// =============================================================================
// MAIN APP CONTROLLER
// =============================================================================

const App = {
    state: {
        currentTier: 1,
        currentView: 'home',
        chartMetric: 'theta_pct',
        chartRangeHours: 24,
        currentSample: null,
        hasData: false,
        selectedZone: null,  // No zone selected by default
        problemFirst: false,
        mapZoom: 1,
        history: [],
        pairedDevices: [],
        allZoneData: {},
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

        await this.loadThresholdsData();
        await this.loadPrefsFromHub();
        this.applyThresholdConfig();
        await this.initFromHardware();
        this.startHardwarePolling();

        // Issue 9: warn if onboarding incomplete
        const prefs = await this.loadPrefsFromHub();
        if (!prefs.onboarding_complete) {
            this.showBanner('warning', 'Setup incomplete — some features may not work correctly.');
            this.showBanner('info', 'Open Settings to complete your crop and soil configuration.');
        }

        this.setupListeners();
        this.bindMobileGestures();
        this.applyStaticTranslations();
        this.renderZoneGrid();
        this.syncBottomNav('home');
        this.updateData();

        Logger.log('UI', 'App initialization complete');
    },

    // Issue 3: fetch current state from hardware on startup
    initFromHardware: async function () {
        try {
            const data = await RealAPI.getLatest();
            if (data) {
                this.state.currentSample = data;
                this.state.hasData = true;
            } else {
                this.state.hasData = false;
            }
        } catch (e) {
            this.state.hasData = false;
            Logger.log('ERROR', 'initFromHardware failed', e);
        }
        await this.refreshAllZoneData();
    },

    refreshAllZoneData: async function () {
        if (!window.HARDWARE_MODE) return;
        try {
            const devices = await RealAPI.getAllZones();
            if (!Array.isArray(devices)) return;
            const paired = devices.filter(d => d.paired === true);
            this.state.pairedDevices = paired;
            const zoneDataMap = {};
            await Promise.all(paired.map(async (device) => {
                const zoneId = device.mac || device.id;
                try {
                    const data = await RealAPI.getLatest(zoneId);
                    if (data) zoneDataMap[zoneId] = data;
                } catch (_) {}
            }));
            this.state.allZoneData = zoneDataMap;
            Logger.log('ZONE', `Loaded data for ${paired.length} paired sensors`);
        } catch (e) {
            Logger.log('ERROR', 'refreshAllZoneData failed', e);
        }
    },

    // Issue 3: poll hardware at 15-minute intervals
    startHardwarePolling: function () {
        setInterval(() => this.updateData(), 15 * 60 * 1000);
    },

    // Issue 12: load prefs from hub (hardware) or localStorage (mock)
    loadPrefsFromHub: async function () {
        if (window.HARDWARE_MODE) {
            try {
                const res = await fetch('/api/config');
                if (res.ok) {
                    const data = await res.json();
                    // cache locally for offline access
                    try { localStorage.setItem('agriscan_user_prefs', JSON.stringify(data)); } catch (_) {}
                    return this.normalizePrefs(data);
                }
            } catch (e) {
                Logger.log('ERROR', 'loadPrefsFromHub failed, using cache', e);
            }
        }
        // Fallback: read from localStorage
        try {
            const raw = localStorage.getItem('agriscan_user_prefs');
            if (raw) return this.normalizePrefs(JSON.parse(raw));
        } catch (_) {}
        return { onboarding_complete: false };
    },

    // Issue 12: save prefs to hub (hardware) or localStorage (mock)
    savePrefsToHub: async function (prefs) {
        if (window.HARDWARE_MODE) {
            try {
                await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(prefs)
                });
                // also cache locally
                try { localStorage.setItem('agriscan_user_prefs', JSON.stringify(prefs)); } catch (_) {}
                return;
            } catch (e) {
                Logger.log('ERROR', 'savePrefsToHub failed', e);
            }
        }
        try {
            localStorage.setItem('agriscan_user_prefs', JSON.stringify(prefs));
        } catch (e) {
            Logger.log('ERROR', 'savePrefsToHub localStorage failed', e);
        }
    },

    updateData: async function () {
        try {
            const sample = await RealAPI.getLatest(this.state.selectedZone);
            if (sample) {
                this.state.currentSample = sample;
                this.state.hasData = true;
                const series = await RealAPI.getSeries(this.state.selectedZone);
                this.state.history = series || [];
                this.render();
            }
        } catch (e) {
            Logger.log('ERROR', 'updateData failed', e);
        }
        await this.refreshAllZoneData();
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
        if (navigator.vibrate) navigator.vibrate(8);
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
        this.openSettingsTab('field');

    },

    openSettingsTab: function (tabName) {
        document.querySelectorAll('.settings-tab').forEach((tab) => {
            tab.classList.toggle('active', tab.dataset.settingsTab === tabName);
        });
        document.querySelectorAll('.settings-panel').forEach((panel) => {
            panel.classList.toggle('active', panel.dataset.settingsPanel === tabName);
        });
        if (tabName === 'device') this.refreshSensorReadings();
    },

    refreshSensorReadings: async function () {
        const set = (id, val) => {
            const el = document.getElementById(id);
            if (el) el.textContent = val;
        };
        set('sr-raw', '…');
        set('sr-theta', '…');
        set('sr-temp', '…');
        set('sr-humidity', '…');
        set('sr-psi', '…');
        set('sr-aw', '…');
        set('sr-status', '…');
        set('sr-updated', '…');
        try {
            const s = await RealAPI.getLatest();
            if (!s) { set('sr-raw', 'No data'); return; }
            const minsAgo = s.timestamp
                ? Math.max(0, Math.round((Date.now() / 1000 - s.timestamp) / 60))
                : null;
            set('sr-raw', s.raw_adc != null ? s.raw_adc : '--');
            set('sr-theta', s.theta != null ? (s.theta * 100).toFixed(1) + '%' : '--');
            set('sr-temp', s.temp_c != null ? s.temp_c.toFixed(1) + '°C' : '--');
            set('sr-humidity', (s.humidity != null && s.humidity >= 0) ? s.humidity.toFixed(1) + '%' : '--');
            set('sr-psi', s.psi_kpa != null ? s.psi_kpa.toFixed(1) + ' kPa' : '--');
            set('sr-aw', s.aw_mm != null ? s.aw_mm.toFixed(1) + ' mm' : '--');
            set('sr-status', s.status || '--');
            set('sr-updated', minsAgo != null ? (minsAgo <= 1 ? 'Just now' : `${minsAgo} min ago`) : '--');
        } catch (e) {
            set('sr-raw', 'Error');
            Logger.log('ERROR', 'refreshSensorReadings failed', e);
        }
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

        this.applyThresholdConfig();
        this.persistCanonicalSettings();
        Logger.log('UI', 'Settings saved', s);
        this.updateData();
        alert("Configuration Saved!");
        this.goToTier(3);
    },

    loadSettings: function () {
        // Issue 12: read from localStorage cache (loadPrefsFromHub handles hardware vs mock)
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
            Logger.log('UI', 'Settings loaded');
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
        // Issue 12: use savePrefsToHub instead of direct localStorage
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
            farmer_name: s.farmer_name || '',
            notes: '',
            plantingDate: s.plantingDate,
            flowRate: s.flowRate
        };
        this.savePrefsToHub(payload);
        try { localStorage.removeItem('agriscan_settings'); } catch (_) {}
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

        // Configure all registered physics engines
        if (typeof window.PhysicsRegistry !== 'undefined') {
            const ids = window.PhysicsRegistry.listIds();
            if (ids.length === 0) ids.push('HUB_ONBOARD');
            ids.forEach(id => {
                const eng = window.PhysicsRegistry.getOrCreate(id);
                eng.configureCropSoil({
                    crop: s.crop,
                    soil: s.soil,
                    planting_ts: s.planting_ts,
                    ...thresholdConfig
                });
            });
        } else if (typeof window.Physics !== 'undefined' && window.Physics.configureCropSoil) {
            window.Physics.configureCropSoil({
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
            this.applyStaticTranslations();
            this.render();
        }
    },

    tr: function (key, params = {}, fallback = '') {
        if (typeof I18n !== 'undefined' && I18n.t) {
            const out = I18n.t(key, params);
            return out === key && fallback ? fallback : out;
        }
        return fallback || key;
    },

    setText: function (id, text) {
        const el = document.getElementById(id);
        if (el) el.textContent = text;
    },

    applyStaticTranslations: function () {
        const T = (k, p, f) => this.tr(k, p, f);
        this.setText('btn-quick-view', T('quick_view'));
        this.setText('btn-field-details', T('field_details'));
        this.setText('lbl-glance-moisture', T('glance_moisture'));
        this.setText('lbl-glance-temp', T('glance_temperature'));
        this.setText('lbl-glance-zones', T('glance_active_sensors'));
        this.setText('lbl-glance-time', T('glance_last_checked'));
        this.setText('t1-footer-note', T('updates_every_few_minutes'));
        this.setText('t2-title', T('todays_field_insight'));
        this.setText('btn-back-home-t2', T('back_to_home'));
        this.setText('btn-open-history-t2', T('open_history'));
        this.setText('btn-back-home-t3', T('back_to_home'));
        this.setText('lbl-t3-soil-moisture', T('soil_moisture'));
        this.setText('lbl-t3-soil-temp', T('soil_temperature'));
        this.setText('lbl-t3-aw', T('available_water'));
        this.setText('lbl-switch-moisture', T('metric_moisture'));
        this.setText('lbl-switch-temp', T('metric_temp'));
        this.setText('lbl-switch-water', T('metric_water'));
        this.setText('lbl-time-range', T('time_range'));
        const rangeSelect = document.getElementById('chart-range-select');
        if (rangeSelect) {
            const opts = Array.from(rangeSelect.options || []);
            if (opts[0]) opts[0].textContent = T('last_24_hours');
            if (opts[1]) opts[1].textContent = T('last_7_days');
            if (opts[2]) opts[2].textContent = T('last_30_days');
        }
        this.setText('lbl-decision', T('watering_decision'));
        this.setText('lbl-field-health', T('field_health'));
        this.setText('lbl-plant-water-access', T('plant_water_access'));
        this.setText('lbl-sensor-confidence', T('sensor_confidence'));
        this.setText('lbl-water-used', T('water_used'));
        this.setText('btn-view-field-map', T('view_field_map'));
        this.setText('btn-download-db', T('download_db'));
        this.setText('btn-view-settings', T('view_settings'));
        this.setText('btn-open-dev-tools', T('open_dev_tools'));
        this.setText('btn-map-back', T('back'));
        this.setText('map-title', T('field_zones'));
        this.setText('map-help', T('map_help'));
        const pfBtn = document.getElementById('btn-problem-first');
        if (pfBtn) pfBtn.childNodes[0].textContent = `${T('problem_zones_first')}: `;
        this.setText('problem-first-state', this.state.problemFirst ? T('problem_first_on') : T('problem_first_off'));
        this.setText('legend-healthy', T('healthy'));
        this.setText('legend-warning', T('warning'));
        this.setText('legend-critical', T('critical'));
        this.setText('legend-no-sensor', T('no_sensor'));
        const sectionTitle = document.getElementById('zone-section-title');
        if (sectionTitle) sectionTitle.childNodes[0].textContent = `${T('field_section')}: `;
        this.setText('lbl-zone-vwc', T('zone_vwc'));
        this.setText('lbl-zone-potential', T('zone_potential'));
        this.setText('lbl-zone-aw', T('zone_avail_h2o'));
        this.setText('lbl-zone-depletion', T('zone_depletion'));
        this.setText('lbl-zone-status', T('zone_status'));
        this.setText('lbl-zone-regime', T('zone_regime'));
        this.setText('zone-trend-title', T('zone_trend_24h'));
        this.setText('btn-clear-zone', T('clear_selection'));
        this.setText('btn-back-dashboard', T('back_to_dashboard'));
        this.setText('btn-settings-back', T('back'));
        this.setText('settings-title', T('settings'));
        this.setText('tab-field', T('field_setup'));
        this.setText('tab-prefs', T('preferences'));
        this.setText('tab-device', T('device_info'));
        this.setText('settings-field-title', T('field_setup'));
        this.setText('lbl-crop-type', T('crop_type'));
        this.setText('lbl-soil-type', T('soil_type'));
        this.setText('lbl-planting-date', T('planting_date'));
        this.setText('lbl-flow-rate', T('flow_rate'));
        this.setText('lbl-farm-area', T('farm_area'));
        this.setText('settings-prefs-title', T('preferences'));
        this.setText('lbl-language', T('language'));
        this.setText('lbl-temp-unit', T('temperature_unit'));
        this.setText('lbl-device-info', T('device_info'));
        this.setText('device-serial', T('device_serial'));
        this.setText('device-firmware', T('device_firmware'));
        this.setText('device-storage', T('device_storage'));
        this.setText('device-troubleshooting', T('device_troubleshooting'));
        this.setText('btn-save-config', T('save_configuration'));
        this.setText('dev-indicator', T('dev_mode'));
        this.setText('dev-tools-title', T('developer_tools'));
        this.setText('btn-dev-close', T('close'));
        this.setText('dev-sim-title', T('simulation'));
        this.setText('btn-sim-rain', T('simulate_rain'));
        this.setText('btn-sim-drought', T('simulate_drought'));
        this.setText('btn-long-run', T('simulation_10m'));
        this.setText('btn-stop-sim', T('stop'));
        this.setText('dev-diag-title', T('diagnostics_exports'));
        this.setText('btn-export-logs', T('export_logs_json'));
        this.setText('btn-export-full', T('export_full_data'));
        this.setText('btn-system-diagnostics', T('system_diagnostics'));
        this.setText('btn-reset-sim', T('reset_simulation'));
        this.setText('nav-home', T('nav_home'));
        this.setText('nav-map', T('nav_field_map'));
        this.setText('nav-history', T('nav_history'));
        this.setText('nav-settings', T('nav_settings'));
        this.setText('chart-trend', `${T('trend_stable')} · --`);
    },

    evaluateMoistureStatus: function (sample) {
        const thetaPct = (sample.theta || 0) * 100;
        const targetLow = Math.max(0, ((sample.theta_refill || (sample.theta_fc || 0.3) * 0.72) * 100) + 2);
        const targetHigh = Math.max(targetLow + 5, (sample.theta_fc || 0.35) * 100);
        const hoursToWatch = sample.dryingRate_per_hr && sample.dryingRate_per_hr < -0.0003
            ? Math.max(1, Math.round(((thetaPct - targetLow) / Math.abs(sample.dryingRate_per_hr * 100)) || 0))
            : 24;

        if (thetaPct < targetLow - 2) {
            return {
                level: 'critical',
                emoji: '🔴',
                status: this.tr('status_critical_short'),
                action: this.tr('action_water_12h'),
                decision: this.tr('decision_yes_soon'),
                decisionDetail: this.tr('decision_detail_critical'),
                targetLow,
                targetHigh,
                hoursToAction: 12
            };
        }
        if (thetaPct < targetLow + 2) {
            const windowHours = Math.min(24, Math.max(4, hoursToWatch));
            const planHours = Math.min(36, Math.max(12, hoursToWatch));
            return {
                level: 'warning',
                emoji: '🟡',
                status: this.tr('status_watch_short'),
                action: this.tr('action_check_hours', { hours: windowHours }),
                decision: this.tr('decision_prepare_soon'),
                decisionDetail: this.tr('decision_detail_warning', { hours: planHours }),
                targetLow,
                targetHigh,
                hoursToAction: planHours
            };
        }
        return {
            level: 'healthy',
            emoji: '🟢',
            status: this.tr('status_healthy_short'),
            action: this.tr('action_no_water_today'),
            decision: this.tr('decision_no_today'),
            decisionDetail: this.tr('decision_detail_healthy'),
            targetLow,
            targetHigh,
            hoursToAction: 36
        };
    },

    resolveUrgency: function (sample) {
        if (!sample) return 'low';
        if (sample.urgency === 'high' || sample.urgency === 'medium' || sample.urgency === 'low') {
            return sample.urgency;
        }

        const status = String(sample.status || '').toLowerCase();
        if (status.includes('critical') || status.includes('dry')) return 'high';
        if (status.includes('warn') || status.includes('watch')) return 'medium';
        if (status.includes('healthy') || status.includes('good')) return 'low';

        const theta = Number(sample.theta || 0);
        const refill = Number(sample.theta_refill || 0);
        const fc = Number(sample.theta_fc || 0);
        if (theta > 0 && refill > 0 && fc > 0) {
            if (theta < refill - 0.01) return 'high';
            if (theta < refill + 0.01) return 'medium';
            return 'low';
        }

        return 'low';
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
        if (!s || !this.state.hasData) {
            document.getElementById('dashboard-waiting')?.classList.remove('hidden');
            return;
        }
        document.getElementById('dashboard-waiting')?.classList.add('hidden');
        const greetingEl = document.getElementById('t1-greeting');
        const statusIcon = document.getElementById('t1-status-icon');
        const msgEl = document.getElementById('t1-message');
        const instrEl = document.getElementById('t1-instruction');
        const weatherEl = document.getElementById('t1-weather');
        const zoneSummaryEl = document.getElementById('t1-zone-summary');
        const timeEl = document.getElementById('t1-time');
        const moistureEl = document.getElementById('t1-glance-moisture');
        const tempEl = document.getElementById('t1-glance-temp');
        const zonesEl = document.getElementById('t1-glance-zones');

        if (!statusIcon || !msgEl) return;

        statusIcon.className = 'status-circle-large';
        const hour = new Date().getHours();
        let greeting = this.tr('greeting_evening');
        if (hour < 12) greeting = this.tr('greeting_morning');
        else if (hour < 17) greeting = this.tr('greeting_afternoon');
        if (greetingEl) greetingEl.textContent = `${greeting} 👋`;
        if (weatherEl) weatherEl.textContent = s.temp_c > 30 ? this.tr('weather_hot') : (s.temp_c < 18 ? this.tr('weather_cool') : this.tr('weather_mild'));

        const moistureState = this.evaluateMoistureStatus(s);

        if (moistureState.level === 'critical') {
            statusIcon.classList.add('critical');
            msgEl.textContent = this.tr('home_msg_critical');
            instrEl.textContent = this.tr('home_instr_critical', { action: moistureState.action });
        } else if (moistureState.level === 'warning') {
            statusIcon.classList.add('warning');
            msgEl.textContent = this.tr('home_msg_warning');
            instrEl.textContent = this.tr('home_instr_warning', { action: moistureState.action });
        } else {
            statusIcon.classList.add('healthy');
            msgEl.textContent = this.tr('home_msg_healthy');
            instrEl.textContent = this.tr('home_instr_healthy');
        }

        if (timeEl) {
            const minsAgo = Math.max(0, Math.round((Date.now() - s.timestamp * 1000) / 60000));
            timeEl.textContent = minsAgo <= 1 ? this.tr('updated_just_now') : this.tr('updated_min_ago', { mins: minsAgo });
        }
        if (moistureEl) moistureEl.textContent = `${moistureState.emoji} ${moistureState.status}`;
        if (tempEl) tempEl.textContent = `${s.temp_c.toFixed(1)}°C`;
        const allZones = Object.values(this.getZoneStatuses());
        const sampledZones = allZones.filter((z) => z.latest).length;
        const healthyZones = allZones.filter((z) => z.latest && this.resolveUrgency(z.latest) === 'low').length;
        if (zonesEl) zonesEl.textContent = this.tr('sensors_count', { count: Object.keys(this.getZoneStatuses()).length });
        if (zoneSummaryEl) {
            if (sampledZones === 0) {
                zoneSummaryEl.textContent = this.tr('collecting_data_from_zones', { count: allZones.length });
            } else {
                zoneSummaryEl.textContent = this.tr('reporting_zones_healthy', { healthy: healthyZones, count: sampledZones });
            }
        }

        if (sampledZones > 0 && healthyZones === 0) {
            statusIcon.className = 'status-circle-large warning';
            msgEl.textContent = this.tr('home_msg_no_healthy');
            instrEl.textContent = this.tr('home_instr_no_healthy');
        }
    },

    renderTier2: function () {
        const s = this.state.currentSample;
        if (!s || !this.state.hasData) {
            document.getElementById('dashboard-waiting')?.classList.remove('hidden');
            return;
        }
        document.getElementById('dashboard-waiting')?.classList.add('hidden');
        const list = document.getElementById('t2-reasons');
        const titleEl = document.querySelector('#tier-2 .view-header h2');

        if (!list) return;
        list.innerHTML = '';

        // Update title based on status
        if (titleEl) {
            const urgency = this.resolveUrgency(s);
            if (urgency === 'high') {
                titleEl.textContent = this.tr('tier2_title_high');
            } else if (urgency === 'medium') {
                titleEl.textContent = this.tr('tier2_title_medium');
            } else {
                titleEl.textContent = this.tr('tier2_title_low');
            }
        }

        const li1 = document.createElement('li');
        li1.innerHTML = `<strong>${this.tr('tier2_moisture_reason', { moisture: (s.theta * 100).toFixed(1), ideal: (s.theta_fc * 100).toFixed(0) })}</strong>`;
        list.appendChild(li1);

        // Yield Risk
        if (this.resolveUrgency(s) === 'high') {
            const liYield = document.createElement('li');
            liYield.style.color = '#C62828';
            const cropMeta = THRESHOLDS_STORE.getCrop(this.state.settings.crop);
            const stageMeta = THRESHOLDS_STORE.getCropStage(this.state.settings.crop, this.state.settings.planting_ts);
            const cropLabel = cropMeta?.display_name || this.state.settings.crop || "Unknown";
            const stageLabel = stageMeta?.name || "current stage";
            liYield.innerHTML = `⚠️ <strong>${this.tr('tier2_crop_stress', { crop: cropLabel, stage: stageLabel })}</strong>`;
            list.appendChild(liYield);
        }

        // Crop Age
        const liStage = document.createElement('li');
        liStage.innerHTML = `<strong>${this.tr('tier2_crop_age', { days: this.getDaysAfterPlanting() })}</strong>`;
        list.appendChild(liStage);

        const li2 = document.createElement('li');
        li2.textContent = this.tr('tier2_water_trend', { trend: s.regime ? s.regime.toLowerCase() : this.tr('still_learning') });
        list.appendChild(li2);

        const liConf = document.createElement('li');
        liConf.innerHTML = `<strong>${this.tr('tier2_model_confidence', { confidence: (s.confidence * 100).toFixed(0) })}</strong>`;
        list.appendChild(liConf);
    },

    renderTier3: function () {
        const s = this.state.currentSample;
        if (!s || !this.state.hasData) {
            document.getElementById('dashboard-waiting')?.classList.remove('hidden');
            return;
        }
        document.getElementById('dashboard-waiting')?.classList.add('hidden');
        const moistureState = this.evaluateMoistureStatus(s);
        const thetaPct = (s.theta * 100);
        const targetText = `${moistureState.targetLow.toFixed(0)}-${moistureState.targetHigh.toFixed(0)}%`;

        this.setSafeText('t3-moist', (s.theta * 100).toFixed(1) + '%');
        this.setSafeText('t3-soil-temp', s.temp_c.toFixed(1) + '°C');
        this.setSafeText('t3-vpd', s.psi_kPa && s.psi_kPa < 40 ? 'Easy' : (s.psi_kPa && s.psi_kPa < 80 ? 'Moderate' : 'Hard'));
        this.setSafeText('t3-confidence', `${Math.round((s.confidence || 0) * 100)}% reliable`);
        this.setSafeText('t3-leaf', s.AW_mm ? `${s.AW_mm.toFixed(1)} mm available` : '--');
        this.setSafeText('t3-depletion', s.fractionDepleted ? `${(s.fractionDepleted * 100).toFixed(0)}% used` : '--');
        const humidityEl = document.getElementById('t3-humidity');
        if (humidityEl) {
            humidityEl.textContent = (s.humidity != null && s.humidity >= 0)
                ? s.humidity.toFixed(1) + '%'
                : '--';
        }
        this.setSafeText('metric-switch-theta', `${thetaPct.toFixed(1)}%`);
        this.setSafeText('metric-switch-temp', `${s.temp_c.toFixed(1)}°C`);
        this.setSafeText('metric-switch-aw', s.AW_mm ? `${s.AW_mm.toFixed(0)}mm` : '--');
        this.setSafeText('t3-moist-context', this.tr('t3_target_status', { low: moistureState.targetLow.toFixed(0), high: moistureState.targetHigh.toFixed(0), status: moistureState.status }));
        this.setSafeText('t3-temp-context', s.temp_c > 32 ? this.tr('t3_crop_comfort_heat') : (s.temp_c < 12 ? this.tr('t3_crop_comfort_cool') : this.tr('t3_crop_comfort_good')));
        this.setSafeText('t3-confidence-context', (s.confidence || 0) >= 0.75 ? this.tr('t3_accuracy_good') : this.tr('t3_accuracy_moderate'));
        this.setSafeText('t3-access-context', s.psi_kPa && s.psi_kPa < 80 ? this.tr('t3_roots_reach_yes') : this.tr('t3_roots_reach_limited'));
        this.setSafeText('t3-aw-context', this.tr('t3_target_reserve', { value: Math.max(20, moistureState.targetLow).toFixed(0) }));
        this.setSafeText('t3-depletion-context', this.tr('t3_since_refill', { value: s.fractionDepleted ? (s.fractionDepleted * 100).toFixed(0) : '--' }));

        const mCell = document.getElementById('t3-moist-cell');
        if (mCell) {
            mCell.className = 'metric-cell';
            if (moistureState.level === 'critical') mCell.classList.add('critical');
            else if (moistureState.level === 'warning') mCell.classList.add('warning');
            else mCell.classList.add('healthy');
        }

        this.setSafeText('decision-need', this.tr('decision_need_line', { decision: moistureState.decision }));
        this.setSafeText('decision-next', this.tr('decision_next_line', { detail: moistureState.decisionDetail, hours: moistureState.hoursToAction }));

        const zones = this.getZoneStatuses();
        let needsAttention = 0;
        Object.values(zones).forEach((z) => {
            if (z.latest) {
                const urgency = this.resolveUrgency(z.latest);
                if (urgency === 'high' || urgency === 'medium') needsAttention++;
            }
        });
        this.setSafeText('health-status', this.tr('health_status_line', { status: moistureState.status }));
        this.setSafeText('health-zone', this.tr('health_zones_line', { count: needsAttention }));

        this.renderChart();
    },

    renderChart: function () {
        const container = document.getElementById('tier3-chart');
        if (!container) return;

        const history = this.state.history;
        if (history.length < 2) return;

        const W = container.clientWidth || 300;
        const H = container.clientHeight || 260;
        const pad = 28;
        const nowTs = Math.floor(Date.now() / 1000);
        const rangeSeconds = (this.state.chartRangeHours || 24) * 3600;

        const metricDefs = {
            theta_pct: {
                title: this.tr('chart_title_moisture'),
                unit: '%',
                value: (h) => (h.theta ?? 0) * 100,
                domain: () => ({ min: 0, max: 50 }),
                status: (v) => (v < 15 ? 'critical' : (v < 20 ? 'warning' : 'healthy')),
                bands: [{ from: 20, to: 50, color: 'rgba(95,141,78,0.10)' }, { from: 15, to: 20, color: 'rgba(244,162,97,0.12)' }, { from: 0, to: 15, color: 'rgba(231,111,81,0.12)' }]
            },
            aw_mm: {
                title: this.tr('chart_title_aw'),
                unit: 'mm',
                value: (h) => h.AW_mm ?? 0,
                domain: (vals) => ({ min: 0, max: Math.max(80, Math.ceil(Math.max(...vals) / 10) * 10) }),
                status: (v) => (v < 20 ? 'critical' : (v < 40 ? 'warning' : 'healthy')),
                bands: [{ from: 40, to: null, color: 'rgba(95,141,78,0.10)' }, { from: 20, to: 40, color: 'rgba(244,162,97,0.12)' }, { from: 0, to: 20, color: 'rgba(231,111,81,0.12)' }]
            },
            depletion_pct: {
                title: this.tr('chart_title_depletion'),
                unit: '%',
                value: (h) => (h.fractionDepleted ?? 0) * 100,
                domain: () => ({ min: 0, max: 100 }),
                status: (v) => (v > 70 ? 'critical' : (v > 40 ? 'warning' : 'healthy')),
                bands: [{ from: 0, to: 40, color: 'rgba(95,141,78,0.10)' }, { from: 40, to: 70, color: 'rgba(244,162,97,0.12)' }, { from: 70, to: 100, color: 'rgba(231,111,81,0.12)' }]
            },
            temp_c: {
                title: this.tr('chart_title_temp'),
                unit: '°C',
                value: (h) => h.temp_c ?? 0,
                domain: (vals) => ({ min: Math.floor(Math.min(...vals, 0) / 5) * 5, max: Math.ceil(Math.max(...vals, 35) / 5) * 5 }),
                status: (v) => (v < 10 || v > 34 ? 'critical' : (v < 15 || v > 30 ? 'warning' : 'healthy')),
                bands: [{ from: 15, to: 30, color: 'rgba(95,141,78,0.10)' }, { from: 10, to: 15, color: 'rgba(244,162,97,0.12)' }, { from: 30, to: 34, color: 'rgba(244,162,97,0.12)' }, { from: 34, to: null, color: 'rgba(231,111,81,0.12)' }]
            }
        };

        const metricKey = this.state.chartMetric || 'theta_pct';
        const metric = metricDefs[metricKey] || metricDefs.theta_pct;
        const rangeStartTs = nowTs - rangeSeconds;
        const filtered = history.filter((h) => h.timestamp >= rangeStartTs && h.timestamp <= nowTs);
        if (filtered.length < 2) {
            container.innerHTML = `<div style="padding:18px;font-size:0.95rem;color:#4f5f56;">${this.tr('chart_not_enough_data_range')}</div>`;
            this.setSafeText('chart-event-1', `• ${this.tr('chart_keep_running')}`);
            this.setSafeText('chart-event-2', `• ${this.tr('chart_try_longer')}`);
            this.setSafeText('chart-event-3', `• ${this.tr('chart_field_average_only')}`);
            return;
        }

        const bucketSec = this.state.chartRangeHours <= 24 ? 1800 : (this.state.chartRangeHours <= 168 ? 10800 : 43200);
        const buckets = new Map();
        filtered.forEach((h) => {
            const bucketTs = Math.floor(h.timestamp / bucketSec) * bucketSec;
            const value = metric.value(h);
            if (!Number.isFinite(value)) return;
            if (!buckets.has(bucketTs)) buckets.set(bucketTs, { sum: 0, count: 0 });
            const entry = buckets.get(bucketTs);
            entry.sum += value;
            entry.count += 1;
        });

        const points = Array.from(buckets.entries())
            .sort((a, b) => a[0] - b[0])
            .map(([timestamp, data]) => ({ timestamp, value: data.sum / Math.max(1, data.count) }));
        if (points.length < 2) {
            container.innerHTML = `<div style="padding:18px;font-size:0.95rem;color:#4f5f56;">${this.tr('chart_need_more_samples')}</div>`;
            return;
        }

        const chartTitle = document.getElementById('chart-title');
        if (chartTitle) chartTitle.textContent = `${metric.title} (${this.state.chartRangeHours || 24}h)`;

        // Stable temperature shortcut
        if (metricKey === 'temp_c') {
            const max = Math.max(...points.map((p) => p.value));
            const min = Math.min(...points.map((p) => p.value));
            if ((max - min) <= 0.8) {
                container.innerHTML = `<div style="padding:18px;font-size:0.95rem;color:#4f5f56;">${this.tr('chart_temp_stable_no_graph', { min: min.toFixed(1), max: max.toFixed(1) })}</div>`;
                const trendElStable = document.getElementById('chart-trend');
                if (trendElStable) trendElStable.textContent = this.tr('trend_stable');
                this.setSafeText('chart-event-1', `• ${this.tr('chart_temp_tight_range')}`);
                this.setSafeText('chart-event-2', `• ${this.tr('chart_no_thermal_stress')}`);
                this.setSafeText('chart-event-3', `• ${this.tr('chart_graph_hidden_noise')}`);
                return;
            }
        }

        const values = points.map((p) => p.value);
        const range = metric.domain(values);
        const tMin = rangeStartTs;
        const tMax = nowTs;
        const getX = (t) => pad + ((t - tMin) / (tMax - tMin + 1)) * (W - 2 * pad);
        const getY = (v) => H - pad - ((v - range.min) / (range.max - range.min || 1)) * (H - 2 * pad);

        const stripEl = document.getElementById('chart-strip');
        const trendEl = document.getElementById('chart-trend');
        let legendBands = '';
        metric.bands.forEach((band) => {
            const yTop = getY(band.to === null ? range.max : band.to);
            const yBottom = getY(band.from);
            legendBands += `<rect x="${pad}" y="${yTop}" width="${W - 2 * pad}" height="${Math.max(0, yBottom - yTop)}" fill="${band.color}" />`;
        });

        const latest = points[points.length - 1];
        const prev = points[points.length - 2];
        const lastDelta = latest.value - prev.value;
        let area = `M ${getX(points[0].timestamp)} ${H - pad} L ${getX(points[0].timestamp)} ${getY(points[0].value)}`;
        let segments = '';
        let markers = '';

        for (let i = 1; i < points.length; i++) {
            const p0 = points[i - 1];
            const p1 = points[i];
            const x0 = getX(p0.timestamp);
            const y0 = getY(p0.value);
            const x1 = getX(p1.timestamp);
            const y1 = getY(p1.value);
            const status = metric.status(p1.value);
            const stroke = status === 'critical' ? '#E76F51' : (status === 'warning' ? '#F4A261' : '#5F8D4E');
            segments += `<line x1="${x0}" y1="${y0}" x2="${x1}" y2="${y1}" stroke="${stroke}" stroke-width="3" stroke-linecap="round" />`;
            area += ` L ${x1} ${y1}`;
            if ((p1.value - p0.value) > (metric.unit === '%' ? 2.0 : 4.0)) {
                markers += `<line x1="${x1}" y1="${pad}" x2="${x1}" y2="${H - pad}" stroke="#7aa2c8" stroke-width="1" stroke-dasharray="3" />`;
            }
        }
        area += ` L ${getX(latest.timestamp)} ${H - pad} Z`;

        const midTs = Math.floor((tMin + tMax) / 2);
        const fmtLabel = (ts) => {
            if ((this.state.chartRangeHours || 24) > 24) {
                return new Date(ts * 1000).toLocaleDateString([], { month: 'short', day: 'numeric' });
            }
            return new Date(ts * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        };
        const startLabel = fmtLabel(tMin);
        const midLabel = fmtLabel(midTs);
        const endLabel = fmtLabel(tMax);
        const yesterdayIdx = Math.max(0, points.length - 96);
        const vsYesterday = latest.value - points[yesterdayIdx].value;
        const projected = latest.value + (lastDelta * 8);
        const latestStatus = metric.status(latest.value);
        const statusText = latestStatus === 'critical' ? this.tr('status_critical_needs_water') : (latestStatus === 'warning' ? this.tr('status_watch_monitor') : this.tr('status_healthy_word'));

        if (trendEl) {
                let trendLabel = this.tr('trend_stable');
                if (lastDelta > 0.3) trendLabel = this.tr('trend_increasing');
                if (lastDelta < -0.3) trendLabel = this.tr('trend_dropping');
                trendEl.textContent = `${trendLabel} · ${this.tr('currently_with_status', { value: latest.value.toFixed(1), unit: metric.unit, status: statusText })} · ${this.tr('vs_yesterday', { value: `${vsYesterday >= 0 ? '+' : ''}${vsYesterday.toFixed(1)}`, unit: metric.unit })} · ${this.tr('forecast_plus_2h', { value: projected.toFixed(1), unit: metric.unit })}`;
            }

        if (stripEl) {
            const step = Math.max(1, Math.floor(points.length / 12));
            let dots = '';
            for (let i = 0; i < points.length; i += step) {
                dots += `<span class="strip-dot ${metric.status(points[i].value)}"></span>`;
            }
            stripEl.innerHTML = `${dots}<span class="strip-time">${startLabel} · ${midLabel} · ${endLabel}</span>`;
        }

        this.setSafeText('chart-event-1', `• ${this.tr('chart_current_line', { value: latest.value.toFixed(1), unit: metric.unit, status: statusText.toLowerCase() })}`);
        this.setSafeText('chart-event-2', `• ${this.tr('chart_peak_low_line', { peak: Math.max(...values).toFixed(1), low: Math.min(...values).toFixed(1), unit: metric.unit })}`);
        this.setSafeText('chart-event-3', `• ${this.tr('chart_forecast_line', { value: projected.toFixed(1), unit: metric.unit })}`);

        container.innerHTML = `
            <svg width="100%" height="100%" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none">
                <defs><linearGradient id="lineFill" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="#7aa67a" stop-opacity="0.45" /><stop offset="100%" stop-color="#7aa67a" stop-opacity="0.02" /></linearGradient></defs>
                ${legendBands}
                ${markers}
                <path d="${area}" fill="url(#lineFill)" stroke="none" />
                ${segments}
                <circle cx="${getX(latest.timestamp)}" cy="${getY(latest.value)}" r="4" fill="#5f8f67"></circle>
                <line x1="${pad}" y1="${H - pad}" x2="${W - pad}" y2="${H - pad}" stroke="#b7c4be" />
                <line x1="${pad}" y1="${pad}" x2="${pad}" y2="${H - pad}" stroke="#b7c4be" />
                <text x="${pad}" y="${H - 5}" font-size="10" fill="#68786f">${startLabel}</text>
                <text x="${W / 2}" y="${H - 5}" text-anchor="middle" font-size="10" fill="#68786f">${midLabel}</text>
                <text x="${W - pad}" y="${H - 5}" text-anchor="end" font-size="10" fill="#68786f">${endLabel}</text>
            </svg>
        `;
    },

    // --- ZONE GRID ---
    getZoneTrendGlyph: function (zoneId) {
        const history = this.getZoneHistory(zoneId);
        if (history.length < 2) return `→ ${this.tr('zone_trend_stable')}`;
        const latest = history[history.length - 1].theta;
        const prev = history[history.length - 2].theta;
        const delta = latest - prev;
        if (delta > 0.005) return `↗ ${this.tr('zone_trend_rising')}`;
        if (delta < -0.005) return `↘ ${this.tr('zone_trend_drying')}`;
        return `→ ${this.tr('zone_trend_stable')}`;
    },

    // Issue 8: zone grid now driven by buildZoneMapFromDevices()
    renderZoneGrid: async function () {
        const container = document.getElementById('zone-grid');
        if (!container) return;

        container.innerHTML = '';

        const result = await buildZoneMapFromDevices();

        // Empty state (no paired devices)
        if (result && result.empty) {
            const msg = document.createElement('div');
            msg.className = 'zone-empty-state';
            msg.textContent = result.message || 'No CropBands paired yet.';
            container.appendChild(msg);
            Logger.log('ZONE', 'Zone grid: empty state');
            return;
        }

        const sensors = Array.isArray(result) ? result : [];
        sensors.forEach(sensor => {
            const cell = document.createElement('div');
            cell.className = 'zone-cell';
            cell.id = `zone-${sensor.id}`;
            const zoneName = sensor.name || ('Zone ' + sensor.id);
            const shortName = zoneName.length > 14 ? `${zoneName.slice(0, 14)}…` : zoneName;
            cell.innerHTML = `
                <div class="zone-id">${shortName}</div>
                <div class="zone-vwc">--</div>
                <div class="zone-trend">--</div>
            `;

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
        const zones = this.getZoneStatuses();

        Object.keys(zones).forEach(zoneId => {
            const cell = document.getElementById(`zone-${zoneId}`);
            if (cell && zones[zoneId].latest) {
                cell.className = 'zone-cell';
                const urgency = this.resolveUrgency(zones[zoneId].latest);

                if (urgency === 'high') cell.classList.add('critical');
                else if (urgency === 'medium') cell.classList.add('warning');
                else cell.classList.add('healthy');

                if (zoneId === this.state.selectedZone) {
                    cell.classList.add('selected');
                }

                const vwcEl = cell.querySelector('.zone-vwc');
                if (vwcEl) vwcEl.textContent = `${(zones[zoneId].latest.theta * 100).toFixed(0)}%`;
                const trendEl = cell.querySelector('.zone-trend');
                if (trendEl) trendEl.textContent = this.getZoneTrendGlyph(zoneId);
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

        const zones = this.getZoneStatuses();
        const zone = zones[this.state.selectedZone];
        if (!zone || !zone.latest) {
            detailsEl.style.display = 'none';
            return;
        }

        const s = zone.latest;
        detailsEl.style.display = 'block';

        // Update zone name
        const nameEl = document.getElementById('zone-name');
        if (nameEl) nameEl.textContent = 'Zone ' + (this.state.selectedZone || '—');

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

        this.renderZoneChart(this.state.selectedZone);

        // Only log on initial selection, not every update
        // Logger is called from selectZone() instead
    },

    renderZoneChart: function (zoneId) {
        const container = document.getElementById('zone-chart');
        const summary = document.getElementById('zone-chart-summary');
        if (!container || !zoneId) return;

        const zoneHistory = this.getZoneHistory(zoneId).slice(-180);
        if (zoneHistory.length < 2) {
            container.innerHTML = `<div style="padding:14px;color:#68786f;font-size:0.9rem;">${this.tr('zone_waiting_data')}</div>`;
            if (summary) summary.textContent = this.tr('zone_trend_after_readings');
            return;
        }

        const W = container.clientWidth || 320;
        const H = container.clientHeight || 180;
        const pad = 24;
        const points = zoneHistory.map((h) => ({ timestamp: h.timestamp, value: (h.theta || 0) * 100 }));
        const values = points.map((p) => p.value);
        const minV = Math.max(0, Math.floor(Math.min(...values) - 3));
        const maxV = Math.min(60, Math.ceil(Math.max(...values) + 3));
        const tMin = points[0].timestamp;
        const tMax = points[points.length - 1].timestamp;
        const getX = (t) => pad + ((t - tMin) / (tMax - tMin + 1)) * (W - 2 * pad);
        const getY = (v) => H - pad - ((v - minV) / (maxV - minV || 1)) * (H - 2 * pad);

        let area = `M ${getX(points[0].timestamp)} ${H - pad} L ${getX(points[0].timestamp)} ${getY(points[0].value)}`;
        let path = `M ${getX(points[0].timestamp)} ${getY(points[0].value)}`;
        for (let i = 1; i < points.length; i++) {
            const x = getX(points[i].timestamp);
            const y = getY(points[i].value);
            path += ` L ${x} ${y}`;
            area += ` L ${x} ${y}`;
        }
        area += ` L ${getX(points[points.length - 1].timestamp)} ${H - pad} Z`;

        const latest = points[points.length - 1];
        const prev = points[points.length - 2];
        const trend = latest.value - prev.value;
        const trendLabel = trend > 0.2 ? this.tr('zone_trend_rising') : (trend < -0.2 ? this.tr('zone_trend_drying') : this.tr('zone_trend_stable'));
        const color = latest.value < 15 ? '#E76F51' : (latest.value < 20 ? '#F4A261' : '#5F8D4E');

        container.innerHTML = `
            <svg width="100%" height="100%" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none">
                <defs><linearGradient id="zoneFill" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="${color}" stop-opacity="0.32" /><stop offset="100%" stop-color="${color}" stop-opacity="0.03" /></linearGradient></defs>
                <path d="${area}" fill="url(#zoneFill)" />
                <path d="${path}" fill="none" stroke="${color}" stroke-width="3" stroke-linecap="round" />
                <circle cx="${getX(latest.timestamp)}" cy="${getY(latest.value)}" r="3.5" fill="${color}" />
                <line x1="${pad}" y1="${H - pad}" x2="${W - pad}" y2="${H - pad}" stroke="#b7c4be" />
                <line x1="${pad}" y1="${pad}" x2="${pad}" y2="${H - pad}" stroke="#b7c4be" />
            </svg>
        `;
        if (summary) summary.textContent = this.tr('zone_current_trend', { value: latest.value.toFixed(1), trend: trendLabel });
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

    toggleProblemFirst: function () {
        this.state.problemFirst = !this.state.problemFirst;
        const stateEl = document.getElementById('problem-first-state');
        if (stateEl) stateEl.textContent = this.state.problemFirst ? this.tr('problem_first_on') : this.tr('problem_first_off');
        this.renderZoneGrid();
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

    exportAllData: function () {
        const data = {
            exportTime: new Date().toISOString(),
            settings: this.state.settings,
            zones: this.getZoneStatuses(),
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

    // Issue 14: extended dev mode toggle
    toggleDevMode: function () {
        const enabled = document.body.classList.toggle('dev-mode');
        if (enabled) {
            this.renderDevDashboard();
        } else {
            this.renderFarmerDashboard();
        }
    },

    renderFarmerDashboard: function () {
        const dev = document.getElementById('dev-dashboard');
        if (dev) dev.style.display = 'none';
    },

    getZoneStatuses: function () {
        const s = this.state.currentSample;
        // Multi-zone: use all paired devices with their individual data
        if (this.state.pairedDevices.length > 0) {
            const result = {};
            this.state.pairedDevices.forEach(device => {
                const zoneId = device.mac || device.id;
                const zoneData = this.state.allZoneData[zoneId] || null;
                const isCurrentZone = s && (s.zoneId === zoneId || (!s.zoneId && !zoneData));
                result[zoneId] = {
                    id: zoneId,
                    active: device.paired !== false,
                    latest: isCurrentZone ? s : zoneData,
                    history: isCurrentZone ? this.state.history : [],
                    battery: device.battery ?? null
                };
            });
            return result;
        }
        // Single-zone fallback
        if (!s) return {};
        const zoneId = s.zoneId || 'HUB';
        return {
            [zoneId]: {
                id: zoneId,
                active: true,
                latest: s,
                history: this.state.history,
                battery: null
            }
        };
    },

    getZoneHistory: function (zoneId) {
        return this.state.history.filter(h => !h.zoneId || h.zoneId === zoneId);
    },

    // Issue 14: render full developer dashboard
    renderDevDashboard: function () {
        const dev = document.getElementById('dev-dashboard');
        if (!dev) return;
        dev.style.display = 'block';

        // Panel 1: Live sensor chart (Chart.js 24h scrolling)
        const ctx = document.getElementById('dev-chart-canvas');
        if (ctx && typeof Chart !== 'undefined') {
            const history = this.state.history || [];
            const labels = history.map(s => new Date(s.timestamp * 1000).toLocaleTimeString());
            const thetaData = history.map(s => (s.theta * 100).toFixed(1));
            {
                if (this._devChart) this._devChart.destroy();
                const calState = window.Physics?.autoCalibration?.getCalibrationState?.() || {};
                const thetaFc = (calState.theta_fc_star || 0.31) * 100;
                const thetaRefill = (calState.theta_refill_star || 0.21) * 100;
                const thetaPwp = (this.state.settings.threshCritical || 14);
                this._devChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels,
                        datasets: [{
                            label: 'Soil VWC %',
                            data: thetaData,
                            borderColor: '#4CAF50',
                            fill: false,
                            tension: 0.3
                        }]
                    },
                    options: {
                        responsive: true,
                        plugins: {
                            annotation: {
                                annotations: {
                                    fcLine: { type: 'line', yMin: thetaFc, yMax: thetaFc, borderColor: 'blue', borderDash: [4, 4], label: { content: 'FC', display: true } },
                                    refillLine: { type: 'line', yMin: thetaRefill, yMax: thetaRefill, borderColor: 'orange', borderDash: [4, 4], label: { content: 'Refill', display: true } },
                                    pwpLine: { type: 'line', yMin: thetaPwp, yMax: thetaPwp, borderColor: 'red', borderDash: [4, 4], label: { content: 'PWP', display: true } }
                                }
                            }
                        }
                    }
                });
            }
        }

        // Panel 2: Calibration state
        const calPanel = document.getElementById('dev-panel-calibration');
        if (calPanel) {
            const calState = window.Physics?.autoCalibration?.getCalibrationState?.() || {};
            const conf = calState.confidence || 0;
            const stage = conf < 0.35 ? 'Learning' : conf < 0.65 ? 'Calibrating' : 'Calibrated';
            calPanel.innerHTML = `
                <div class="dev-cal-stage">Stage: <strong>${stage}</strong></div>
                <div class="dev-cal-bar-wrap"><div class="dev-cal-bar" style="width:${(conf*100).toFixed(0)}%"></div></div>
                <div class="dev-cal-meta">Confidence: ${(conf*100).toFixed(1)}% &nbsp;|&nbsp; Events: ${calState.n_events||0} &nbsp;|&nbsp; FC updates: ${calState.n_fc_updates||0}</div>
            `;
        }

        // Panel 3: Physics event log
        const logPanel = document.getElementById('dev-panel-log');
        if (logPanel) {
            const events = typeof PhysicsEventLogger !== 'undefined' ? PhysicsEventLogger.getEvents() : [];
            logPanel.innerHTML = events.length === 0
                ? '<em>No physics events yet.</em>'
                : events.slice(-50).map(e => `<div class="dev-log-entry">[${e.type}] ${e.message || JSON.stringify(e)}</div>`).join('');
            logPanel.scrollTop = logPanel.scrollHeight;
            const clearBtn = document.getElementById('dev-log-clear');
            if (clearBtn) clearBtn.onclick = () => { if (typeof PhysicsEventLogger !== 'undefined') PhysicsEventLogger.clear?.(); this.renderDevDashboard(); };
        }

        // Panel 4: Per-device panel
        const devicePanel = document.getElementById('dev-panel-devices');
        if (devicePanel) {
            const ids = window.PhysicsRegistry?.listIds?.() || ['HUB_ONBOARD'];
            devicePanel.innerHTML = ids.map(id => {
                const eng = window.PhysicsRegistry?.get(id);
                const cal = eng?.autoCalibration?.getCalibrationState?.() || {};
                return `<div class="dev-device-row"><strong>${id}</strong> — conf: ${((cal.confidence||0)*100).toFixed(0)}% events: ${cal.n_events||0}</div>`;
            }).join('');
        }

        // Panel 5: Storage panel
        this.renderStoragePanel();
    },

    // Issue 13: storage management panel
    renderStoragePanel: async function () {
        const panel = document.getElementById('dev-panel-storage');
        if (!panel) return;

        try {
            const res = await fetch('/api/storage');
            if (!res.ok) throw new Error('storage API error');
            const data = await res.json();
            const pct = data.total_mb > 0 ? Math.round(data.used_mb / data.total_mb * 100) : 0;
            const warnClass = pct >= 95 ? 'storage-danger' : pct >= 80 ? 'storage-warn' : '';
            panel.innerHTML = `
                <div class="dev-storage-bar-wrap ${warnClass}">
                    <div class="dev-storage-bar" style="width:${pct}%"></div>
                </div>
                <div class="dev-storage-meta">${data.used_mb.toFixed(1)} MB used / ${data.free_mb.toFixed(1)} MB free (${pct}%)</div>
                ${pct >= 80 ? `<div class="dev-storage-warning ${pct >= 95 ? 'danger' : 'warn'}">${pct >= 95 ? '⚠️ Critical: SD card nearly full!' : '⚠️ SD card usage above 80%'}</div>` : ''}
                <div class="dev-storage-actions">
                    <button onclick="App.downloadLogs()">Download Logs</button>
                    <button onclick="App.clearLogs()">Clear Logs</button>
                </div>
            `;
        } catch (e) {
            panel.innerHTML = `<em>Storage unavailable: ${e.message}</em>`;
        }
    },

    downloadLogs: async function () {
        window.location.href = '/api/logs/download';
    },

    clearLogs: async function () {
        if (!confirm('Clear all sensor logs? This cannot be undone.')) return;
        try {
            await fetch('/api/logs/clear', { method: 'DELETE' });
            this.renderStoragePanel();
        } catch (e) {
            Logger.log('ERROR', 'clearLogs failed', e);
        }
    },

    showBanner: function (type, message) {
        // Issue 9: display informational banners
        const container = document.getElementById('banner-container') || document.body;
        const banner = document.createElement('div');
        banner.className = `app-banner app-banner-${type}`;
        banner.textContent = message;
        const close = document.createElement('button');
        close.textContent = '×';
        close.onclick = () => banner.remove();
        banner.appendChild(close);
        container.insertBefore(banner, container.firstChild);
    },

    bindMobileGestures: function () {
        let startX = 0;
        let startY = 0;
        let pullTriggered = false;
        let lastPinchDistance = null;

        const zoneGrid = document.getElementById('zone-grid');
        if (zoneGrid) {
            zoneGrid.addEventListener('touchstart', (e) => {
                if (e.touches.length === 1) {
                    startX = e.touches[0].clientX;
                    startY = e.touches[0].clientY;
                }
                if (e.touches.length === 2) {
                    const [a, b] = e.touches;
                    lastPinchDistance = Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY);
                }
            }, { passive: true });

            zoneGrid.addEventListener('touchmove', (e) => {
                if (e.touches.length === 2 && lastPinchDistance) {
                    const [a, b] = e.touches;
                    const dist = Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY);
                    const ratio = dist / lastPinchDistance;
                    this.state.mapZoom = Math.min(1.35, Math.max(0.9, this.state.mapZoom * ratio));
                    zoneGrid.style.transform = `scale(${this.state.mapZoom.toFixed(2)})`;
                    zoneGrid.style.transformOrigin = 'center top';
                    lastPinchDistance = dist;
                }
            }, { passive: true });

            zoneGrid.addEventListener('touchend', (e) => {
                if (e.changedTouches.length === 1) {
                    const endX = e.changedTouches[0].clientX;
                    const endY = e.changedTouches[0].clientY;
                    const dx = endX - startX;
                    const dy = endY - startY;
                    if (Math.abs(dx) > 55 && Math.abs(dx) > Math.abs(dy)) {
                        this.cycleZone(dx < 0 ? 1 : -1);
                    }
                }
                if (e.touches.length < 2) lastPinchDistance = null;
            });
        }

        document.addEventListener('touchstart', (e) => {
            if (window.scrollY === 0 && e.touches.length === 1) {
                startY = e.touches[0].clientY;
                pullTriggered = false;
            }
        }, { passive: true });

        document.addEventListener('touchmove', (e) => {
            if (window.scrollY === 0 && e.touches.length === 1) {
                const deltaY = e.touches[0].clientY - startY;
                if (deltaY > 90 && !pullTriggered) {
                    pullTriggered = true;
                    this.updateData();
                    this.refreshSensorData();
                    Logger.log('UI', 'Pull-to-refresh triggered');
                }
            }
        }, { passive: true });

        document.querySelectorAll('.metric-cell').forEach((cell) => {
            let timer = null;
            cell.addEventListener('touchstart', () => {
                timer = setTimeout(() => {
                    const label = cell.querySelector('label')?.textContent || 'Metric';
                    alert(`${label}: This value summarizes field water status for quick decisions.`);
                }, 650);
            }, { passive: true });
            cell.addEventListener('touchend', () => {
                if (timer) clearTimeout(timer);
                timer = null;
            });
            cell.addEventListener('touchcancel', () => {
                if (timer) clearTimeout(timer);
                timer = null;
            });
        });
    },

    cycleZone: function (direction) {
        const activeIds = Object.keys(this.getZoneStatuses());
        if (activeIds.length === 0) return;
        const currentIdx = this.state.selectedZone ? activeIds.indexOf(this.state.selectedZone) : 0;
        const nextIdx = ((currentIdx + direction) % activeIds.length + activeIds.length) % activeIds.length;
        this.selectZone(activeIds[nextIdx]);
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

        document.querySelectorAll('#chart-metric-pills [data-metric]').forEach((pill) => {
            pill.classList.toggle('active', pill.dataset.metric === this.state.chartMetric);
            pill.addEventListener('click', () => {
                this.state.chartMetric = pill.dataset.metric;
                document.querySelectorAll('#chart-metric-pills [data-metric]').forEach((el) => el.classList.remove('active'));
                pill.classList.add('active');
                this.renderChart();
            });
        });
        const rangeSelect = document.getElementById('chart-range-select');
        if (rangeSelect) {
            rangeSelect.value = String(this.state.chartRangeHours || 24);
            rangeSelect.addEventListener('change', () => {
                this.state.chartRangeHours = Number(rangeSelect.value) || 24;
                this.renderChart();
            });
        }

        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.shiftKey && e.key.toLowerCase() === 'd') {
                e.preventDefault();
                this.toggleDevMode();
            }
        });
    }
};

// =============================================================================
// GLOBAL EXPORTS
// =============================================================================

// Track app start time for uptime calculations
window.appStartTime = Date.now();

window.App = App;
window.Logger = Logger;
window.PhysicsEventLogger = PhysicsEventLogger;

if (!window.__devDashboard) {
    document.addEventListener('DOMContentLoaded', () => App.init());
}
