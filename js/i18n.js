/**
 * I18n System
 * Simple translation handler.
 */

const I18n = {
    lang: 'en',

    strings: {
        en: {
            "status_irrigate": "WATER YOUR CROPS",
            "status_check": "CHECK FIELD SOON",
            "status_good": "ALL GOOD",
            "action_irrigate": "Open valve for {hours} hours",
            "action_check": "Moisture is dropping",
            "action_good": "No action needed",
            "why_title": "Why do I need to water?",
            "reason_moisture_low": "Moisture is {moisture}% (Target: >{critical}%)",
            "reason_drying_fast": "Drying rate is high ({rate}%/hr)",
            "reason_stress": "Crop stress begins in {hours} hours"
        },
        es: {
            "status_irrigate": "RIEGA TUS CULTIVOS",
            "status_check": "REVISAR PRONTO",
            "status_good": "TODO BIEN",
            "action_irrigate": "Abre la válvula por {hours} horas",
            "action_check": "La humedad está bajando",
            "action_good": "No se requiere acción",
            "why_title": "¿Por qué necesito regar?",
            "reason_moisture_low": "Humedad al {moisture}% (Meta: >{critical}%)",
            "reason_drying_fast": "Tasa de secado alta ({rate}%/hr)",
            "reason_stress": "Estrés del cultivo en {hours} horas"
        }

    },

    init: function () {
        const saved = localStorage.getItem('agriscan_lang');
        if (saved && this.strings[saved]) {
            this.lang = saved;
        }
    },

    setLang: function (code) {
        if (this.strings[code]) {
            this.lang = code;
            localStorage.setItem('agriscan_lang', code);
            return true;
        }
        return false;
    },

    t: function (key, params = {}) {
        let str = this.strings[this.lang][key] || key;

        // Replace params {key}
        Object.keys(params).forEach(k => {
            str = str.replace(`{${k}}`, params[k]);
        });

        return str;
    }
};

window.I18n = I18n;
