# AgriScan Project: Implemented Features

This document tracks all features built into the AgriScan offline-first dashboard, updated for the Tiered Architecture.

## ✅ Core Architecture
- **Tiered UX**: Progressive disclosure model (Simple -> Reasoning -> Expert).
- **Offline Architecture**: Zero dependencies, single HTML bundle.
- **Physics Engine**: Simplified logic for VPD and ET0 calculations.
- **Translation System**: Internal i18n support.

## ✅ Smart Logic (Deep Features)
- **Crop Profiles**: Selecting "Rice" or "Wheat" automatically calibrates critical moisture thresholds.
- **Growth Stage**: Calculates "Days After Planting" from user settings.
- **Yield Risk**: Dynamic text warns of yield loss risk during critical crop stages.
- **Irrigation Detection**: Visualizes watering events (spikes) on the history chart.

## ✅ Tier 1: Simple Mode
- **Visual Status**: Large, high-contrast LED indicator.
- **Primary Instruction**: Clear, singular action message (e.g., "WATER CROPS").
- **Navigation**: intuitive buttons to access deeper layers.
- **Language**: Instant toggle.

## ✅ Tier 2: Reasoning Mode
- **Plain Language Logic**: Explains *why* the status is what it is.
- **Consequence Engine**: Explains *what happens* if you don't act (Yield Impact).

## ✅ Tier 3: Expert Mode
- **Data Grid**: 9-Metric dashboard including Leaf Wetness, Depletion, Balance.
- **Visualization**: SVG charts with Healthy Bands and Irrigation Markers.
- **Export**: Full CSV download.

## ✅ Configuration (Settings)
- **Quick Setup**: Crop type and planting date selectors.
- **Advanced Tuning**: Calibration for flow limits, area size, and critical thresholds.
- **Persistence**: LocalStorage saving of all user preferences.
