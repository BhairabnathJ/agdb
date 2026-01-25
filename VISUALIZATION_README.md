# AgriScan Simulation Visualization Guide

This guide explains how to run simulations, export data, and generate visualizations from the AgriScan Dashboard.

## Quick Start

### 1. Run the Dashboard

Open `index.html` in your web browser:

```bash
# Option 1: Direct file
open index.html

# Option 2: With a local server (recommended)
python -m http.server 8000
# Then visit: http://localhost:8000
```

### 2. Run a Simulation

1. Navigate to **Tier 3** (Show Details)
2. Scroll to the **Data Simulator** section
3. Choose a simulation:
   - **Simulate Rain**: Quick rain event (30 seconds)
   - **Simulate Drought**: Progressive drying (1 minute)
   - **10-Min Simulation**: Full 10-minute simulation with multiple phases

### 3. Monitor Progress

- Open browser console (F12 or Cmd+Option+I) to see detailed logging
- Watch the countdown timer during long simulations
- Logs are color-coded:
  - 🔵 Blue: Physics calculations
  - 🟣 Purple: Simulation events
  - 🟢 Green: UI updates
  - 🟠 Orange: Data operations
  - 🟡 Yellow: Zone updates

### 4. Export Data

After simulation completes, click:
- **Export Logs (JSON)**: Download simulation logs as `simulation_logs.json`
- **Export Full Data**: Download complete dataset with all metrics

### 5. Generate Visualizations

Install Python dependencies:

```bash
pip install -r requirements.txt
```

Run the visualization script:

```bash
python visualize_simulation.py simulation_logs.json
```

This generates 11 graphs + 1 report in a timestamped session folder:
- `graph_vwc.png` - Volumetric Water Content over time
- `graph_psi.png` - Matric Potential (soil water tension)
- `graph_aw.png` - Available Water depth
- `graph_depletion.png` - Soil water depletion percentage
- `graph_status.png` - Zone status distribution
- `graph_phases.png` - Phase timeline with average VWC
- `graph_raw.png` - Raw ADC sensor readings
- `graph_confidence.png` - **Calibration confidence over time** ⭐ NEW
- `graph_drying_rate.png` - **Soil moisture change rate** ⭐ NEW
- `graph_temperature.png` - **Soil temperature trends** ⭐ NEW
- `graph_dashboard.png` - **Multi-metric dashboard (6-panel overview)** ⭐ NEW
- `simulation_report.txt` - Statistical summary

All files are saved to `graphs/session_YYYYMMDD_HHMMSS/`

## Simulation Details

### 10-Minute Long Run Phases

The long simulation cycles through 7 realistic phases:

1. **Baseline** (0-1 min): Stable initial conditions
2. **Morning Drying** (1-3 min): Gradual soil drying from ET
3. **Irrigation Event** (3-3.5 min): Rapid wetting from irrigation
4. **Post-Irrigation Drainage** (3.5-5 min): Gravitational drainage
5. **Midday Drying** (5-7 min): Faster drying with peak ET
6. **Evening Stabilization** (7-8.5 min): Slower drying as ET decreases
7. **Night Recovery** (8.5-10 min): Minimal change, slight redistribution

### Zone Configuration

The simulation uses a 4×4 grid with 9 active sensors:
- **Active zones**: A1, A2, A4, B1, B3, C2, C4, D2, D3
- **Inactive zones**: A3, B2, B4, C1, C3, D1, D4

Each zone has independent soil properties and responds differently to environmental changes.

## Output Organization

Each visualization run creates a new timestamped folder:
```
graphs/
├── session_20260125_143022/
│   ├── graph_vwc.png
│   ├── graph_confidence.png
│   ├── graph_dashboard.png
│   └── ... (11 more files)
├── session_20260125_150845/
│   └── ... (another session)
```

This allows you to:
- Compare different simulation runs
- Keep historical analysis organized
- Prevent overwriting previous results

## Interpreting the Graphs

### VWC (Volumetric Water Content)
- **Y-axis**: Percentage of soil volume that is water
- **Healthy range**: > 45%
- **Warning**: 35-45%
- **Critical**: < 35%
- **Pattern**: Should increase with rain/irrigation, decrease with ET

### Matric Potential (Psi)
- **Y-axis**: Soil water tension in kPa (negative values)
- **Interpretation**: More negative = harder for plants to extract water
- **Typical range**: -10 kPa (wet) to -1500 kPa (wilting point)
- **Inverted axis**: Graph shows easier water availability at top

### Available Water (AW)
- **Y-axis**: Depth of plant-available water in mm
- **Calculation**: Based on VWC between field capacity and wilting point
- **Usage**: Direct indication of irrigation needs

### Depletion Percentage
- **Y-axis**: Percentage of available water depleted
- **MAD threshold**: 50% (Management Allowed Depletion)
- **Critical**: 75%+ depletion
- **Action**: Irrigate when approaching MAD threshold

### Status Distribution
- **Stacked area chart**: Shows how many zones are in each status
- **Green**: Healthy zones
- **Yellow**: Warning zones
- **Red**: Critical zones
- **Pattern**: Should shift during irrigation events

### Phase Timeline
- **Top**: Timeline showing simulation phases
- **Bottom**: Average VWC during each phase
- **Colors**: Blue (wetting), Red (drying), Gray (stable)

### Calibration Confidence ⭐ NEW
- **Y-axis**: Confidence percentage (0-100%)
- **Interpretation**: How confident the physics engine is in its calibration
- **Thresholds**:
  - 50% = Moderate confidence (yellow line)
  - 80% = High confidence (green line)
- **Expected behavior**: Should increase over time as auto-calibration learns
- **Low confidence**: May indicate sensor issues or unusual soil conditions

### Drying Rate ⭐ NEW
- **Y-axis**: Rate of moisture change (% VWC per hour)
- **Positive values**: Soil is wetting (irrigation, rain)
- **Negative values**: Soil is drying (evapotranspiration)
- **Zero line**: No change in moisture
- **Thresholds**:
  - > +0.2%/hr = Rapid wetting (irrigation event)
  - < -0.2%/hr = Rapid drying (high ET stress)
- **Usage**: Predict when irrigation will be needed based on drying trends

### Soil Temperature ⭐ NEW
- **Y-axis**: Temperature in °C
- **Interpretation**: Soil temperature affects:
  - Sensor readings (temperature compensation)
  - Evapotranspiration rates
  - Root water uptake
- **Typical range**: 15-30°C
- **Pattern**: Should follow diurnal (day/night) cycles

### Raw ADC Readings
- **Y-axis**: Raw analog-to-digital converter values (0-1023)
- **Interpretation**: Direct sensor output before calibration
- **Lower values**: Drier soil (less capacitance)
- **Higher values**: Wetter soil (more capacitance)
- **Usage**: Diagnose sensor hardware issues

### Multi-Metric Dashboard ⭐ NEW
- **6-panel overview**: All key metrics in one view
- **Shows averages** across all active zones
- **Perfect for**: Quick assessment of overall field status
- **Panels**:
  1. VWC - Current moisture levels
  2. Psi - Water availability to plants
  3. AW - Remaining water reserves
  4. Depletion - How much water has been used
  5. Confidence - System reliability
  6. Drying Rate - Trend direction

## Console Logging

The dashboard logs comprehensive data during simulations:

```
[SIMULATION] Started: Long Run Simulation (10:00)
[ZONE] Zone A1 updated: VWC=42.5%, Status=healthy
[PHYSICS] Calculated for 9 zones in 12ms
[SIMULATION] Phase: morning_drying (1:23 / 10:00)
[DATA] Exported 300 log entries
```

Log categories:
- `PHYSICS`: Soil physics calculations
- `SIMULATION`: Simulation control and phases
- `UI`: User interface updates
- `DATA`: Data export operations
- `ZONE`: Individual zone updates
- `I18N`: Translation system
- `ERROR`: Error messages
- `CALIBRATION`: Auto-calibration events

## Troubleshooting

### No data in graphs
- Ensure simulation completed successfully
- Check that `simulation_logs.json` contains zone data
- Verify file path is correct

### Missing Python packages
```bash
pip install matplotlib numpy
```

### Simulation stops unexpectedly
- Check browser console for errors
- Verify physics.js is loaded
- Try shorter simulations first (Rain/Drought)

### Translation not working
- Verify i18n.js is loaded
- Check language selector sync
- Reload page and try again

## Data Format

Exported JSON structure:
```json
[
  {
    "timestamp": "2026-01-25T10:30:15.234Z",
    "elapsed_ms": 0,
    "simulation": "long_run",
    "phase": "baseline",
    "zones": {
      "A1": {
        "active": true,
        "theta": 0.425,
        "psi_kPa": -45.2,
        "AW_mm": 87.3,
        "fractionDepleted": 0.12,
        "status": "healthy",
        "regime": "stable"
      }
    }
  }
]
```

## Advanced Usage

### Custom Visualizations

Modify `visualize_simulation.py` to create custom graphs:

```python
# Example: Plot only specific zones
zones_to_plot = ['A1', 'B3', 'C2']
filtered_data = {k: v for k, v in zones_data.items() if k in zones_to_plot}
plot_vwc_over_time(filtered_data, 'custom_vwc.png')
```

### Real-time Monitoring

For live monitoring, export logs periodically:
```javascript
// In browser console
setInterval(() => App.exportSimulationLogs(), 60000); // Every minute
```

### Comparison Analysis

Compare multiple simulation runs:
```bash
python visualize_simulation.py rain_event.json
python visualize_simulation.py drought_event.json
# Compare the generated graphs
```

## Next Steps

1. Run all three simulation types
2. Compare the resulting graphs
3. Analyze how different phases affect soil moisture
4. Test zone mapping visualization (View Zone Map button)
5. Experiment with custom simulations by modifying phase parameters

## Support

For issues or questions:
- Check browser console for error messages
- Verify all files are in correct locations
- Ensure physics.js, app.js, and i18n.js are loaded
- Review specification-v2.md for system architecture

---

**Generated for AgriScan Dashboard** | Low-cost Agricultural IoT Monitoring
