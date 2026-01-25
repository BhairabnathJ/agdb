/**
 * Physics Engine (Stub)
 * Handles physics calculations for the AgriScan dashboard.
 * 
 * NOTE: This is a simplified version as requested.
 * The user will implement the full physics logic later.
 */

const Physics = {
    // Calculate Vapor Pressure Deficit (kPa)
    calculateVPD: function (tempC, humidity) {
        if (tempC == null || humidity == null) return 0;
        // Simple mock approximation
        // SVP = 0.6108 * exp(17.27 * T / (T + 237.3))
        const svp = 0.6108 * Math.exp((17.27 * tempC) / (tempC + 237.3));
        const avp = svp * (humidity / 100);
        return parseFloat((svp - avp).toFixed(2));
    },

    // Calculate Reference Evapotranspiration (mm/day)
    calculateET0: function (tempC) {
        // Stub: Returns a value based loosely on temp
        return parseFloat((tempC * 0.2).toFixed(1));
    },

    // Calculate Soil Drying Rate (%/hr)
    calculateDryingRate: function (currentMoisture, tempC) {
        // Stub: Drying is faster when hotter
        return parseFloat((tempC * 0.05).toFixed(2));
    },

    // Determine Action Status
    decideAction: function (moisture, thresholdCritical, thresholdWarning) {
        if (moisture < thresholdCritical) return 'IRRIGATE_NOW';
        if (moisture < thresholdWarning) return 'CHECK_SOON';
        return 'ALL_GOOD';
    },

    // Calculate Time to Critical (hours)
    calculateTimeToCritical: function (currentMoisture, criticalLevel, dryingRate) {
        if (dryingRate <= 0) return 999;
        if (currentMoisture <= criticalLevel) return 0;
        return Math.round((currentMoisture - criticalLevel) / dryingRate);
    }
};

// Expose to global scope for Phase 0 simplicity (no modules)
window.Physics = Physics;
