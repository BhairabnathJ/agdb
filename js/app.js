/**
 * AgriScan App - Final Feature Complete
 * Supports Tier 1/2/3, Leaf Wetness, SVG Charts, Export, Settings.
 * + Deep Logic: Crop Profiles, Growth Stages, Yield Risk, Irrigation Markers.
 */

const CROP_DATA = {
    'maize': { crit: 30, warn: 50, risk: "High (Silking Stage)" },
    'rice': { crit: 50, warn: 70, risk: "Critical (Panicle Init)" },
    'wheat': { crit: 20, warn: 40, risk: "Moderate (Tillering)" },
    'veg': { crit: 35, warn: 55, risk: "Very High (Fruiting)" },
    'custom': { crit: 30, warn: 50, risk: "Unknown" }
};

const App = {
    state: {
        currentTier: 1,
        activeZone: 0,
        zones: [],
        settings: {
            tempUnit: 'c',
            threshCritical: 30,
            threshWarning: 50,
            crop: 'maize',
            plantingDate: null
        },
        decision: 'ALL_GOOD',
        physics: {}
    },

    // --- MOCK API (Simulating ESP32 Web Server) ---
    const MockAPI = {
        db: [], // In-memory "SQLite"

        inject: function (raw_adc, temp_c) {
            const ts = Math.floor(Date.now() / 1000);
            const sample = Physics.processSensorReading(raw_adc, temp_c, ts);
            this.db.push(sample);
            // Keep DB size managed
            if (this.db.length > 500) this.db.shift();
            return sample;
        },

        // Seed initial data
        seed: function () {
            let now = Math.floor(Date.now() / 1000);
            let raw = 2200; // Start somewhere middle
            for (let i = 0; i < 50; i++) {
                const t = now - (50 - i) * 600; // Every 10 mins
                raw += (Math.random() - 0.5) * 50;
                this.db.push(Physics.processSensorReading(raw, 24, t));
            }
        },

        getCurrent: async function () {
            if (this.db.length === 0) this.seed();
            return this.db[this.db.length - 1];
        },

        getSeries: async function (start, end) {
            return this.db; // Return all for now
        }
    };


    // --- MAIN APP CONTROLLER ---
    const App = {
        state: {
            currentTier: 1,
            currentSample: null, // Holds the rich physics object
            history: [],
            settings: {
                tempUnit: 'c',
                crop: 'maize'
            }
        },

        init: function () {
            I18n.init();
            MockAPI.seed(); // Start Mock DB
            this.setupListeners();

            // Polling Loop
            this.updateData();
            setInterval(() => this.updateData(), 2000); // Fast polling for simulator feel
        },

        updateData: async function () {
            const sample = await MockAPI.getCurrent();
            this.state.currentSample = sample;
            this.state.history = await MockAPI.getSeries();
            this.render();
        },

        // --- SIMULATION ACTIONS ---
        simulateRain: function () {
            // Inject rapid wetting
            let raw = this.state.currentSample ? this.state.currentSample.raw_adc : 3000;
            let count = 0;
            const interval = setInterval(() => {
                raw -= 150; // Lower ADC = Wetter
                MockAPI.inject(raw, 24);
                count++;
                if (count > 10) clearInterval(interval);
                this.updateData(); // Force refresh UI
            }, 200); // Fast burst
        },

        simulateDrought: function () {
            // One big dry step
            let raw = this.state.currentSample ? this.state.currentSample.raw_adc : 2000;
            raw += 500; // Drier
            MockAPI.inject(raw, 28);
            this.updateData();
        },

        // --- ROUTING / HELPERS ---
        goToTier: function (tier) {
            this.state.currentTier = tier;
            document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
            document.getElementById(`tier-${tier}`).classList.add('active');
            if (tier === 3) this.renderTier3();
        },

        goToSettings: function () {
            document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
            document.getElementById('settings').classList.add('active');

            const s = this.state.settings;
            if (document.getElementById('set-crop')) document.getElementById('set-crop').value = s.crop || 'maize';
            if (document.getElementById('set-date')) document.getElementById('set-date').value = s.plantingDate;
            if (document.getElementById('set-flow')) document.getElementById('set-flow').value = s.flowRate || 1000;
        },

        saveSettings: function () {
            const s = this.state.settings;

            // Crop & Auto-Thresholds (Deep Logic)
            const cropEl = document.getElementById('set-crop');
            if (cropEl) {
                s.crop = cropEl.value;
                if (CROP_DATA[s.crop]) {
                    s.threshCritical = CROP_DATA[s.crop].crit;
                    s.threshWarning = CROP_DATA[s.crop].warn;
                }
            }

            const dateEl = document.getElementById('set-date');
            if (dateEl) s.plantingDate = dateEl.value;

            const flowEl = document.getElementById('set-flow');
            if (flowEl) s.flowRate = parseFloat(flowEl.value);

            // Crit slider manual override (optional)
            const critEl = document.getElementById('set-crit-slide');
            if (critEl) s.threshCritical = parseInt(critEl.value);

            localStorage.setItem('agriscan_settings', JSON.stringify(s));
            this.updateLoop();
            alert("Configuration Saved! Thresholds updated for " + s.crop);
            this.goToTier(3);
        },

        loadSettings: function () {
            const saved = localStorage.getItem('agriscan_settings');
            if (saved) {
                try {
                    this.state.settings = { ...this.state.settings, ...JSON.parse(saved) };
                } catch (e) { console.warn("Settings parse error", e); }
            }
        },

        getDaysAfterPlanting: function () {
            if (!this.state.settings.plantingDate) return 0;
            const start = new Date(this.state.settings.plantingDate);
            const now = new Date();
            const diff = now - start;
            return Math.floor(diff / (1000 * 60 * 60 * 24));
        },

        setLang: function (code) {
            if (I18n.setLang(code)) this.render();
        },

        // --- RENDERING ---
        render: function () {
            if (!this.state.currentSample) return;
            this.renderTier1();
            this.renderTier2();
            if (this.state.currentTier === 3) this.renderTier3();
        },

        renderTier1: function () {
            const s = this.state.currentSample;
            const statusIcon = document.getElementById('t1-status-icon');
            const msgEl = document.getElementById('t1-message');
            const instrEl = document.getElementById('t1-instruction');
            const timeEl = document.getElementById('t1-time');

            statusIcon.className = 'status-circle-large';

            // Map Schema 'urgency' to UI
            if (s.urgency === 'high') {
                statusIcon.classList.add('critical');
                msgEl.textContent = s.status; // "CRITICAL"
                instrEl.textContent = "Irrigation Required";
            } else if (s.urgency === 'medium') {
                statusIcon.classList.add('warning');
                msgEl.textContent = s.status;
                instrEl.textContent = "Monitor Closely";
            } else {
                statusIcon.classList.add('healthy');
                msgEl.textContent = s.status;
                instrEl.textContent = "Conditions Optimal";
            }

            if (timeEl) timeEl.textContent = "Updated: " + new Date(s.timestamp * 1000).toLocaleTimeString();
        },

        renderTier2: function () {
            const s = this.state.currentSample;
            const list = document.getElementById('t2-reasons');
            list.innerHTML = '';

            const li1 = document.createElement('li');
            li1.innerHTML = `<strong>VWC is ${(s.theta * 100).toFixed(1)}%</strong>. (FC: ${(s.theta_fc * 100).toFixed(0)}%)`;
            list.appendChild(li1);

            const li2 = document.createElement('li');
            li2.textContent = `Regime: ${s.regime.toUpperCase()}`;
            list.appendChild(li2);

            const li3 = document.createElement('li');
            li3.textContent = `Calibration Confidence: ${(s.confidence * 100).toFixed(0)}%`;
            list.appendChild(li3);
        },

        renderTier3: function () {
            const s = this.state.currentSample;

            this.setSafeText('t3-moist', (s.theta * 100).toFixed(1) + '%'); // VWC
            this.setSafeText('t3-soil-temp', s.temp_c.toFixed(1) + '°C');
            this.setSafeText('t3-air-temp', '--');
            this.setSafeText('t3-humid', '--');
            this.setSafeText('t3-vpd', s.psi_kpa + ' kPa'); // Using VPD slot for Psi for now
            this.setSafeText('t3-ec', s.raw_adc); // Show Raw ADC in EC slot
            this.setSafeText('t3-leaf', (s.aw_mm).toFixed(0) + ' AW');
            this.setSafeText('t3-depletion', (s.fraction_depleted * 100).toFixed(0) + '%');
            this.setSafeText('t3-balance', '--');

            const mCell = document.getElementById('t3-moist-cell');
            if (s.urgency === 'high') mCell.className = 'metric-cell critical';
            else if (s.urgency === 'medium') mCell.className = 'metric-cell warning';
            else mCell.className = 'metric-cell healthy';

            // Update labels via JS if needed or rely on HTML update
            this.renderChart();
        },

        renderChart: function () {
            // Render from this.state.history
            const container = document.getElementById('tier3-chart');
            if (!container) return;

            const history = this.state.history;
            if (history.length < 2) return;

            const W = container.clientWidth || 300;
            const H = container.clientHeight || 200;
            const pad = 20;

            const tMin = history[0].timestamp;
            const tMax = history[history.length - 1].timestamp;
            // Map Theta 0.0-0.5
            const vMin = 0; const vMax = 0.5;

            const getX = t => pad + ((t - tMin) / (tMax - tMin)) * (W - 2 * pad);
            const getY = v => H - pad - ((v - vMin) / (vMax - vMin)) * (H - 2 * pad);

            let d = `M ${getX(history[0].timestamp)} ${getY(history[0].theta)}`;
            for (let i = 1; i < history.length; i++) {
                d += ` L ${getX(history[i].timestamp)} ${getY(history[i].theta)}`;
            }

            container.innerHTML = `
      <svg width="100%" height="100%">
        <!-- Grid -->
        <line x1="${pad}" y1="${getY(0.32)}" x2="${W - pad}" y2="${getY(0.32)}" stroke="blue" stroke-width="1" stroke-dasharray="4" />
        <text x="${W - pad}" y="${getY(0.32) - 5}" text-anchor="end" font-size="9" fill="blue">FC</text>

        <line x1="${pad}" y1="${getY(0.12)}" x2="${W - pad}" y2="${getY(0.12)}" stroke="red" stroke-width="1" stroke-dasharray="4" />
        <text x="${W - pad}" y="${getY(0.12) - 5}" text-anchor="end" font-size="9" fill="red">WP</text>

        <path d="${d}" fill="none" stroke="#2E7D32" stroke-width="2" />
        <line x1="${pad}" y1="${H - pad}" x2="${W - pad}" y2="${H - pad}" stroke="#ccc" />
        <line x1="${pad}" y1="${pad}" x2="${pad}" y2="${H - pad}" stroke="#ccc" />
      </svg>
    `;
        },
    },

    exportCSV: function () {
        let csv = "ZoneID,Metric,Value,Unit\n";
        this.state.zones.forEach(z => {
            csv += `${z.id},Moisture,${z.moisture.toFixed(1)},%\n`;
            csv += `${z.id},LeafWet,${z.leafWetness.toFixed(1)},%\n`;
            csv += `${z.id},Temp,${z.temp.toFixed(1)},C\n`;
        });
        const blob = new Blob([csv], { type: 'text/csv' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `agriscan_${Date.now()}.csv`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
    },

    setSafeText: function (id, text) {
        const el = document.getElementById(id);
        if (el) el.textContent = text;
    },

    setupListeners: function () {
        const btnC = document.getElementById('btn-c');
        const btnF = document.getElementById('btn-f');
        if (btnC && btnF) {
            btnC.addEventListener('click', () => { this.state.settings.tempUnit = 'c'; this.render(); });
            btnF.addEventListener('click', () => { this.state.settings.tempUnit = 'f'; this.render(); });
        }

        const sel = document.getElementById('t3-zone-select');
        if (sel) {
            this.state.zones.forEach((z, i) => {
                const opt = document.createElement('option');
                opt.value = i;
                opt.textContent = z.name;
                sel.appendChild(opt);
            });
            sel.addEventListener('change', (e) => {
                this.state.activeZone = parseInt(e.target.value);
                this.runPhysicsEngine();
                this.render();
            });
        }
    }
};

window.App = App;
document.addEventListener('DOMContentLoaded', () => App.init());
