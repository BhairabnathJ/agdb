/**
 * I18n System
 * Simple translation handler with full logging.
 */

const I18n = {
    lang: 'en',

    strings: {
        en: {
            // Tier 1 Status
            "status_irrigate": "WATER YOUR CROPS",
            "status_check": "CHECK FIELD SOON",
            "status_good": "ALL GOOD",
            "action_irrigate": "Open valve for {hours} hours",
            "action_check": "Moisture is dropping",
            "action_good": "No action needed",
            // Tier 2 Reasoning
            "why_title_irrigate": "Why do I need to water?",
            "why_title_check": "Why should I check the field?",
            "why_title_good": "Why is everything good?",
            "reason_moisture_low": "Moisture is {moisture}% (Target: >{critical}%)",
            "reason_drying_fast": "Drying rate is high ({rate}%/hr)",
            "reason_stress": "Crop stress begins in {hours} hours",
            "reason_optimal": "Soil moisture is in the optimal range",
            "reason_stable": "Conditions are stable, no action needed",
            // Zone Map
            "zone_title": "Field Zones",
            "zone_active": "{count} sensors active",
            "zone_legend_optimal": "Optimal",
            "zone_legend_warning": "Warning",
            "zone_legend_critical": "Critical",
            // General
            "updated": "Updated: {time}",
            "confidence": "Confidence: {value}%"
        },
        es: {
            // Tier 1 Status
            "status_irrigate": "RIEGA TUS CULTIVOS",
            "status_check": "REVISAR PRONTO",
            "status_good": "TODO BIEN",
            "action_irrigate": "Abre la válvula por {hours} horas",
            "action_check": "La humedad está bajando",
            "action_good": "No se requiere acción",
            // Tier 2 Reasoning
            "why_title_irrigate": "¿Por qué necesito regar?",
            "why_title_check": "¿Por qué debo revisar el campo?",
            "why_title_good": "¿Por qué está todo bien?",
            "reason_moisture_low": "Humedad al {moisture}% (Meta: >{critical}%)",
            "reason_drying_fast": "Tasa de secado alta ({rate}%/hr)",
            "reason_stress": "Estrés del cultivo en {hours} horas",
            "reason_optimal": "La humedad del suelo está en el rango óptimo",
            "reason_stable": "Condiciones estables, no se necesita acción",
            // Zone Map
            "zone_title": "Zonas del Campo",
            "zone_active": "{count} sensores activos",
            "zone_legend_optimal": "Óptimo",
            "zone_legend_warning": "Advertencia",
            "zone_legend_critical": "Crítico",
            // General
            "updated": "Actualizado: {time}",
            "confidence": "Confianza: {value}%"
        }
    },

    init: function () {
        const saved = localStorage.getItem('agriscan_lang');
        if (saved && this.strings[saved]) {
            this.lang = saved;
            console.log(`[I18n] Loaded saved language: ${saved}`);
        } else {
            console.log(`[I18n] Using default language: ${this.lang}`);
        }
        this.syncSelectors();
    },

    setLang: function (code) {
        if (this.strings[code]) {
            this.lang = code;
            localStorage.setItem('agriscan_lang', code);
            console.log(`[I18n] Language changed to: ${code}`);
            this.syncSelectors();
            return true;
        }
        console.warn(`[I18n] Unknown language code: ${code}`);
        return false;
    },

    // Sync all language selectors in the UI
    syncSelectors: function () {
        const selectors = document.querySelectorAll('#lang-select-t1, #set-lang');
        selectors.forEach(sel => {
            if (sel) sel.value = this.lang;
        });
    },

    t: function (key, params = {}) {
        let str = this.strings[this.lang][key];

        if (!str) {
            console.warn(`[I18n] Missing translation for key: "${key}" in language: ${this.lang}`);
            return key; // Return key as fallback
        }

        // Replace params {key}
        Object.keys(params).forEach(k => {
            str = str.replace(new RegExp(`\\{${k}\\}`, 'g'), params[k]);
        });

        return str;
    },

    // Get current language
    getLang: function () {
        return this.lang;
    }
};

window.I18n = I18n;
