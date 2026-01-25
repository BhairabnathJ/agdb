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

    init: function () {
        I18n.init();
        this.loadSettings();

        // Enforce planting date default for mock if missing
        if (!this.state.settings.plantingDate) {
            const d = new Date();
            d.setDate(d.getDate() - 45); // Mock 45 days ago
            this.state.settings.plantingDate = d.toISOString().split('T')[0];
        }

        // Enforce crop thresholds sync on load
        const c = this.state.settings.crop;
        if (CROP_DATA[c]) {
            this.state.settings.threshCritical = CROP_DATA[c].crit;
            this.state.settings.threshWarning = CROP_DATA[c].warn;
        }

        this.generateMockZones();
        this.setupListeners();
        this.updateLoop();
        setInterval(() => this.updateLoop(), 10000);
    },

    // --- MOCK DATA ---
    generateMockZones: function () {
        const zones = [];
        for (let i = 0; i < 16; i++) {
            zones.push({
                id: i,
                name: `Zone ${String.fromCharCode(65 + Math.floor(i / 4))}${i % 4 + 1}`,
                moisture: 45,
                temp: 24,
                humidity: 60,
                ec: 1.2,
                leafWetness: 20
            });
        }
        this.state.zones = zones;
    },

    // --- CORE LOOP ---
    updateLoop: function () {
        this.simulateSensorChanges();
        this.runPhysicsEngine();
        this.render();
    },

    simulateSensorChanges: function () {
        this.state.zones.forEach(z => {
            z.moisture += (Math.random() - 0.5) * 1.5;
            z.moisture = Math.max(10, Math.min(90, z.moisture));
            z.temp += (Math.random() - 0.5) * 0.2;
            z.leafWetness = Math.max(0, Math.min(100, z.leafWetness + (Math.random() - 0.5) * 5));
        });
    },

    runPhysicsEngine: function () {
        const z = this.state.zones[this.state.activeZone];

        const vpd = Physics.calculateVPD(z.temp, z.humidity);
        const et0 = Physics.calculateET0(z.temp);
        const rate = Physics.calculateDryingRate(z.moisture, z.temp);

        const depletion = Math.max(0, (40 - z.moisture) / (40 - 10) * 100);

        let risk = 'LOW';
        if (z.humidity > 80 && z.leafWetness > 50) risk = 'HIGH';
        else if (z.humidity > 70) risk = 'MODERATE';

        const balance = (Math.random() * 5 - et0).toFixed(1);

        const decision = Physics.decideAction(z.moisture, this.state.settings.threshCritical, this.state.settings.threshWarning);
        const timeToCrit = Physics.calculateTimeToCritical(z.moisture, this.state.settings.threshCritical, rate);

        this.state.physics = { vpd, et0, rate, timeToCrit, risk, depletion, balance };
        this.state.decision = decision;
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
        this.renderTier1();
        this.renderTier2();
        if (this.state.currentTier === 3) this.renderTier3();
    },

    renderTier1: function () {
        const statusIcon = document.getElementById('t1-status-icon');
        const msgEl = document.getElementById('t1-message');
        const instrEl = document.getElementById('t1-instruction');
        const timeEl = document.getElementById('t1-time');

        statusIcon.className = 'status-circle-large';

        switch (this.state.decision) {
            case 'IRRIGATE_NOW':
                statusIcon.classList.add('critical');
                msgEl.textContent = I18n.t('status_irrigate');
                instrEl.textContent = I18n.t('action_irrigate', { hours: 2 });
                break;
            case 'CHECK_SOON':
                statusIcon.classList.add('warning');
                msgEl.textContent = I18n.t('status_check');
                instrEl.textContent = I18n.t('action_check');
                break;
            default:
                statusIcon.classList.add('healthy');
                msgEl.textContent = I18n.t('status_good');
                instrEl.textContent = I18n.t('action_good');
        }

        if (timeEl) timeEl.textContent = "Last updated: Just now";
    },

    renderTier2: function () {
        const z = this.state.zones[this.state.activeZone];
        const list = document.getElementById('t2-reasons');
        list.innerHTML = '';
        const phys = this.state.physics;

        // 1. Context
        const li1 = document.createElement('li');
        li1.innerHTML = `<strong>Moisture is ${Math.round(z.moisture)}%</strong>. (Target: >${this.state.settings.threshWarning}%)`;
        list.appendChild(li1);

        // 2. Yield Risk
        if (this.state.decision === 'IRRIGATE_NOW') {
            const liYield = document.createElement('li');
            liYield.style.color = '#C62828';
            const cropRisk = CROP_DATA[this.state.settings.crop] ? CROP_DATA[this.state.settings.crop].risk : "Unknown";
            liYield.innerHTML = `⚠️ <strong>Yield Risk: ${cropRisk}</strong>. Immediate water stress.`;
            list.appendChild(liYield);
        }

        // 3. Growth Stage
        const liStage = document.createElement('li');
        const days = this.getDaysAfterPlanting();
        liStage.innerHTML = `Crop Age: <strong>${days} days</strong>`;
        list.appendChild(liStage);

        // 4. Rate
        if (this.state.decision !== 'ALL_GOOD') {
            const li2 = document.createElement('li');
            li2.textContent = I18n.t('reason_drying_fast', { rate: phys.rate });
            list.appendChild(li2);
        }
    },

    renderTier3: function () {
        const z = this.state.zones[this.state.activeZone];
        const phys = this.state.physics;

        this.setSafeText('t3-moist', Math.round(z.moisture) + '%');
        this.setSafeText('t3-soil-temp', z.temp.toFixed(1) + '°C');
        this.setSafeText('t3-air-temp', (z.temp + 2).toFixed(1) + '°C');
        this.setSafeText('t3-humid', z.humidity.toFixed(0) + '%');
        this.setSafeText('t3-vpd', phys.vpd);
        this.setSafeText('t3-ec', z.ec);
        this.setSafeText('t3-leaf', z.leafWetness.toFixed(0) + '%');
        this.setSafeText('t3-depletion', Math.round(phys.depletion) + '%');
        this.setSafeText('t3-balance', phys.balance + 'mm');

        this.setSafeText('t3-drying', phys.rate + '%/hr');
        this.setSafeText('t3-et0', phys.et0 + ' mm');
        this.setSafeText('t3-risk', phys.risk);

        const reasonText = `Moisture is ${Math.round(z.moisture)}% (Target >${this.state.settings.threshWarning}%). ${CROP_DATA[this.state.settings.crop].risk} risk stage.`;
        this.setSafeText('t3-reason-text', reasonText);

        const mCell = document.getElementById('t3-moist-cell');
        mCell.className = 'metric-cell';
        if (this.state.decision === 'IRRIGATE_NOW') mCell.classList.add('critical');
        else if (this.state.decision === 'CHECK_SOON') mCell.classList.add('warning');
        else mCell.classList.add('healthy');

        this.renderChart();
    },

    renderChart: function () {
        const container = document.getElementById('tier3-chart');
        if (!container) return;

        // History Gen
        const history = [];
        const now = new Date().getTime();
        let val = this.state.zones[this.state.activeZone].moisture;
        for (let i = 0; i < 24; i++) {
            history.unshift({
                ts: now - (i * 3600 * 1000),
                val: Math.max(10, Math.min(90, val + (Math.random() - 0.5) * 10))
            });
        }

        const W = container.clientWidth || 300;
        const H = container.clientHeight || 200;
        const pad = 20;

        const tMin = history[0].ts;
        const tMax = history[history.length - 1].ts;
        const vMin = 0; const vMax = 100;

        const getX = t => pad + ((t - tMin) / (tMax - tMin)) * (W - 2 * pad);
        const getY = v => H - pad - ((v - vMin) / (vMax - vMin)) * (H - 2 * pad);

        const y50 = getY(50);
        const y70 = getY(70);
        const bandPath = `M ${pad},${y70} L ${W - pad},${y70} L ${W - pad},${y50} L ${pad},${y50} Z`;

        let d = `M ${getX(history[0].ts)} ${getY(history[0].val)}`;
        let markers = '';

        for (let i = 1; i < history.length; i++) {
            d += ` L ${getX(history[i].ts)} ${getY(history[i].val)}`;
            // Irrigation Detection (Jump > 5% positive)
            if ((history[i].val - history[i - 1].val) > 5) {
                const x = getX(history[i].ts);
                markers += `<line x1="${x}" y1="${pad}" x2="${x}" y2="${H - pad}" stroke="#2196F3" stroke-width="1" stroke-dasharray="2" />`;
                markers += `<text x="${x}" y="${pad - 5}" text-anchor="middle" font-size="9" fill="#2196F3">💧</text>`;
            }
        }

        container.innerHTML = `
      <svg width="100%" height="100%">
        <path d="${bandPath}" fill="#E8F5E9" stroke="none" />
        <text x="${W - pad}" y="${y70 - 5}" text-anchor="end" font-size="10" fill="#4CAF50">Optimal</text>
        <line x1="${pad}" y1="${getY(30)}" x2="${W - pad}" y2="${getY(30)}" stroke="#FFEBEE" stroke-width="1" stroke-dasharray="4" />
        ${markers}
        <path d="${d}" fill="none" stroke="#2E7D32" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" />
        <line x1="${pad}" y1="${H - pad}" x2="${W - pad}" y2="${H - pad}" stroke="#ccc" />
        <line x1="${pad}" y1="${pad}" x2="${pad}" y2="${H - pad}" stroke="#ccc" />
      </svg>
    `;
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
