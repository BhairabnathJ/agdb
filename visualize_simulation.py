#!/usr/bin/env python3
"""
AgriScan Simulation Data Visualizer

This script reads simulation data exported from the AgriScan dashboard
and generates multiple visualization graphs for analysis.

Usage:
    python visualize_simulation.py [simulation_logs.json]

If no file is specified, it will look for 'simulation_logs.json' in the current directory.
"""

import json
import sys
from pathlib import Path
from datetime import datetime, timedelta
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from collections import defaultdict
import numpy as np
import os

# Color scheme matching AgriScan dashboard
COLORS = {
    'healthy': '#2E7D32',
    'warning': '#F9A825',
    'critical': '#C62828',
    'neutral': '#666666',
    'zones': ['#1976D2', '#388E3C', '#D32F2F', '#F57C00', '#7B1FA2',
              '#C2185B', '#00796B', '#5D4037', '#455A64']
}

def create_session_directory():
    """Create a timestamped session directory for output graphs."""
    # Create main graphs directory
    base_dir = Path('graphs')
    base_dir.mkdir(exist_ok=True)

    # Create session subdirectory with timestamp
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    session_dir = base_dir / f'session_{timestamp}'
    session_dir.mkdir(exist_ok=True)

    return session_dir

def load_simulation_data(filename='simulation_logs.json'):
    """Load simulation data from JSON file."""
    filepath = Path(filename)

    if not filepath.exists():
        print(f"❌ Error: File '{filename}' not found.")
        print(f"   Please export simulation logs from the AgriScan dashboard.")
        sys.exit(1)

    with open(filepath, 'r') as f:
        data = json.load(f)

    print(f"✅ Loaded {len(data)} data points from {filename}")
    return data

def extract_zone_data(data):
    """Extract data organized by zone from flattened format."""
    zones_data = defaultdict(lambda: {
        'timestamps': [],
        'time_ms': [],
        'vwc': [],
        'psi': [],
        'aw': [],
        'depletion': [],
        'status': [],
        'regime': [],
        'urgency': [],
        'phase': [],
        'raw': [],
        'temp': [],
        'confidence': [],
        'drying_rate': []
    })

    for entry in data:
        zone_id = entry.get('zone')
        if not zone_id:
            continue

        # Extract time
        time_ms = entry.get('time', 0)
        elapsed_min = entry.get('elapsed_min', time_ms / 60000)

        # Store all data
        zones_data[zone_id]['timestamps'].append(elapsed_min)
        zones_data[zone_id]['time_ms'].append(time_ms)
        zones_data[zone_id]['vwc'].append(entry.get('theta', 0) * 100)
        zones_data[zone_id]['psi'].append(float(entry.get('psi_kPa', 0)))
        zones_data[zone_id]['aw'].append(float(entry.get('AW_mm', 0)))

        # Calculate depletion if available
        theta = entry.get('theta', 0)
        theta_fc = 0.35  # Approximate field capacity
        theta_pwp = 0.15  # Permanent wilting point
        if theta_fc > theta_pwp:
            depletion = max(0, min(100, ((theta_fc - theta) / (theta_fc - theta_pwp)) * 100))
        else:
            depletion = 0
        zones_data[zone_id]['depletion'].append(depletion)

        zones_data[zone_id]['status'].append(entry.get('status', 'unknown'))
        zones_data[zone_id]['regime'].append(entry.get('regime', 'unknown'))
        zones_data[zone_id]['urgency'].append(entry.get('urgency', 'none'))
        zones_data[zone_id]['phase'].append(entry.get('phase', ''))
        zones_data[zone_id]['raw'].append(entry.get('raw', 0))
        zones_data[zone_id]['temp'].append(float(entry.get('temp', 0)))

        # Confidence (may be string or float)
        conf = entry.get('confidence', 0)
        if isinstance(conf, str):
            try:
                conf = float(conf)
            except:
                conf = 0
        zones_data[zone_id]['confidence'].append(conf * 100)  # Convert to percentage

        # Drying rate (may be string or float, can be null)
        dr = entry.get('dryingRate_per_hr', 0)
        if dr is None:
            dr = 0
        elif isinstance(dr, str):
            try:
                dr = float(dr)
            except:
                dr = 0
        zones_data[zone_id]['drying_rate'].append(dr)

    return zones_data

def get_phase_data(data):
    """Extract phase timeline from data."""
    phases = []
    seen_phases = set()

    for entry in data:
        phase = entry.get('phase', '')
        tick = entry.get('tick', 0)
        time_ms = entry.get('time', 0)
        elapsed_min = entry.get('elapsed_min', time_ms / 60000)

        key = f"{tick}_{phase}"
        if key not in seen_phases and phase:
            phases.append({
                'name': phase,
                'tick': tick,
                'time_min': elapsed_min
            })
            seen_phases.add(key)

    return phases

def plot_vwc_over_time(zones_data, output_file='graph_vwc.png'):
    """Plot Volumetric Water Content over time for all zones."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(zone_data['timestamps'], zone_data['vwc'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='o',
                   markersize=2,
                   alpha=0.8)

    # Reference lines
    ax.axhline(y=15, color=COLORS['critical'], linestyle='--', alpha=0.5, label='Critical threshold')
    ax.axhline(y=25, color=COLORS['warning'], linestyle='--', alpha=0.5, label='Warning threshold')

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Volumetric Water Content (%)', fontsize=12, fontweight='bold')
    ax.set_title('VWC Over Time - All Zones', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_matric_potential(zones_data, output_file='graph_psi.png'):
    """Plot matric potential (soil water tension) over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(zone_data['timestamps'], zone_data['psi'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='s',
                   markersize=2,
                   alpha=0.8)

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Matric Potential (kPa)', fontsize=12, fontweight='bold')
    ax.set_title('Soil Water Tension Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_available_water(zones_data, output_file='graph_aw.png'):
    """Plot available water depth over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(zone_data['timestamps'], zone_data['aw'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='^',
                   markersize=2,
                   alpha=0.8)

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Available Water (mm)', fontsize=12, fontweight='bold')
    ax.set_title('Available Water Depth Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_depletion_percentage(zones_data, output_file='graph_depletion.png'):
    """Plot soil water depletion percentage over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(zone_data['timestamps'], zone_data['depletion'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='d',
                   markersize=2,
                   alpha=0.8)

    # Reference lines
    ax.axhline(y=50, color=COLORS['warning'], linestyle='--', alpha=0.5, label='MAD threshold (50%)')
    ax.axhline(y=75, color=COLORS['critical'], linestyle='--', alpha=0.5, label='Critical depletion')

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Depletion (%)', fontsize=12, fontweight='bold')
    ax.set_title('Soil Water Depletion Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 100)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_status_distribution(data, output_file='graph_status.png'):
    """Plot distribution of status categories over time."""
    # Group by tick to get status counts at each time point
    tick_data = defaultdict(lambda: {'time_min': 0, 'healthy': 0, 'warning': 0, 'critical': 0, 'unknown': 0})

    for entry in data:
        tick = entry.get('tick', 0)
        time_ms = entry.get('time', 0)
        elapsed_min = entry.get('elapsed_min', time_ms / 60000)
        urgency = entry.get('urgency', 'none')

        tick_data[tick]['time_min'] = elapsed_min

        if urgency == 'high':
            tick_data[tick]['critical'] += 1
        elif urgency == 'medium':
            tick_data[tick]['warning'] += 1
        elif urgency == 'low' or urgency == 'none':
            tick_data[tick]['healthy'] += 1
        else:
            tick_data[tick]['unknown'] += 1

    if not tick_data:
        print("⚠️  No status data available for plotting")
        return

    # Sort by tick
    sorted_ticks = sorted(tick_data.items())
    time_points = [t[1]['time_min'] for t in sorted_ticks]
    healthy_counts = [t[1]['healthy'] for t in sorted_ticks]
    warning_counts = [t[1]['warning'] for t in sorted_ticks]
    critical_counts = [t[1]['critical'] for t in sorted_ticks]

    fig, ax = plt.subplots(figsize=(12, 6))

    # Stacked area chart
    ax.fill_between(time_points, 0, healthy_counts,
                     color=COLORS['healthy'], alpha=0.7, label='Healthy')
    ax.fill_between(time_points, healthy_counts,
                     np.array(healthy_counts) + np.array(warning_counts),
                     color=COLORS['warning'], alpha=0.7, label='Warning')
    ax.fill_between(time_points,
                     np.array(healthy_counts) + np.array(warning_counts),
                     np.array(healthy_counts) + np.array(warning_counts) + np.array(critical_counts),
                     color=COLORS['critical'], alpha=0.7, label='Critical')

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Number of Zones', fontsize=12, fontweight='bold')
    ax.set_title('Zone Status Distribution Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=10)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_phase_timeline(data, zones_data, output_file='graph_phases.png'):
    """Plot simulation phases over time."""
    phases = get_phase_data(data)

    if not phases:
        print("⚠️  No phase data available for plotting")
        return

    # Create phase visualization
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), height_ratios=[1, 3])

    # Get unique phases in order
    unique_phases = []
    phase_starts = []
    phase_colors_map = {}

    current_phase = None
    for p in phases:
        if p['name'] != current_phase:
            unique_phases.append(p['name'])
            phase_starts.append(p['time_min'])
            current_phase = p['name']

            # Assign color based on phase name
            if 'irrigation' in p['name'].lower() or 'wetting' in p['name'].lower():
                phase_colors_map[p['name']] = COLORS['healthy']
            elif 'drying' in p['name'].lower() or 'drought' in p['name'].lower() or 'drydown' in p['name'].lower():
                phase_colors_map[p['name']] = COLORS['critical']
            else:
                phase_colors_map[p['name']] = COLORS['neutral']

    # Add end time
    if zones_data:
        max_time = max([max(z['timestamps']) for z in zones_data.values() if z['timestamps']])
        phase_starts.append(max_time)
    else:
        phase_starts.append(phases[-1]['time_min'] + 1)

    # Draw phase blocks
    for i in range(len(unique_phases)):
        start = phase_starts[i]
        end = phase_starts[i + 1]
        ax1.barh(0, end - start, left=start, height=0.8,
                color=phase_colors_map.get(unique_phases[i], COLORS['neutral']),
                alpha=0.7,
                edgecolor='black',
                linewidth=1)
        # Add phase label
        mid = (start + end) / 2
        label_text = unique_phases[i].replace('_', ' ').title()
        ax1.text(mid, 0, label_text,
                ha='center', va='center', fontsize=9, fontweight='bold')

    ax1.set_xlim(0, phase_starts[-1])
    ax1.set_ylim(-0.5, 0.5)
    ax1.set_yticks([])
    ax1.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax1.set_title('Simulation Phase Timeline', fontsize=14, fontweight='bold', pad=20)
    ax1.grid(True, axis='x', alpha=0.3)

    # Average VWC during each phase
    phase_avg_vwc = defaultdict(list)
    for entry in data:
        phase_name = entry.get('phase', '')
        theta = entry.get('theta', 0)
        if phase_name and theta:
            phase_avg_vwc[phase_name].append(theta * 100)

    phase_names = list(phase_avg_vwc.keys())
    avg_vwc_values = [np.mean(phase_avg_vwc[p]) if phase_avg_vwc[p] else 0 for p in phase_names]

    bars = ax2.bar(range(len(phase_names)), avg_vwc_values,
                   color=[phase_colors_map.get(p, COLORS['neutral']) for p in phase_names],
                   alpha=0.7,
                   edgecolor='black',
                   linewidth=1)

    ax2.set_xticks(range(len(phase_names)))
    ax2.set_xticklabels([p.replace('_', ' ').title() for p in phase_names],
                        rotation=45, ha='right', fontsize=9)
    ax2.set_ylabel('Average VWC (%)', fontsize=12, fontweight='bold')
    ax2.set_title('Average VWC by Phase', fontsize=12, fontweight='bold')
    ax2.grid(True, axis='y', alpha=0.3)

    # Add value labels on bars
    for i, (bar, val) in enumerate(zip(bars, avg_vwc_values)):
        ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                f'{val:.1f}%', ha='center', va='bottom', fontsize=9, fontweight='bold')

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_raw_adc_values(zones_data, output_file='graph_raw.png'):
    """Plot raw ADC sensor readings over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0 and zone_data['raw']:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(zone_data['timestamps'], zone_data['raw'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='.',
                   markersize=2,
                   alpha=0.8)

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Raw ADC Reading', fontsize=12, fontweight='bold')
    ax.set_title('Raw Sensor Readings Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_confidence_over_time(zones_data, output_file='graph_confidence.png'):
    """Plot calibration confidence over time for all zones."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0 and zone_data['confidence']:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(zone_data['timestamps'], zone_data['confidence'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='o',
                   markersize=2,
                   alpha=0.8)

    # Reference lines
    ax.axhline(y=50, color=COLORS['warning'], linestyle='--', alpha=0.5, label='Moderate confidence')
    ax.axhline(y=80, color=COLORS['healthy'], linestyle='--', alpha=0.5, label='High confidence')

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Calibration Confidence (%)', fontsize=12, fontweight='bold')
    ax.set_title('Calibration Confidence Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 100)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_drying_rate(zones_data, output_file='graph_drying_rate.png'):
    """Plot soil drying rate over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0 and zone_data['drying_rate']:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            # Filter out zeros for cleaner visualization
            filtered_times = []
            filtered_rates = []
            for t, r in zip(zone_data['timestamps'], zone_data['drying_rate']):
                if r != 0:
                    filtered_times.append(t)
                    filtered_rates.append(r * 100)  # Convert to percentage per hour

            if filtered_times:
                ax.plot(filtered_times, filtered_rates,
                       label=f'Zone {zone_id}',
                       linewidth=2,
                       color=color,
                       marker='.',
                       markersize=2,
                       alpha=0.8)

    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5, alpha=0.5)
    ax.axhline(y=-0.2, color=COLORS['critical'], linestyle='--', alpha=0.5, label='Rapid drying')
    ax.axhline(y=0.2, color=COLORS['healthy'], linestyle='--', alpha=0.5, label='Rapid wetting')

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Drying Rate (% VWC/hour)', fontsize=12, fontweight='bold')
    ax.set_title('Soil Moisture Change Rate', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_temperature(zones_data, output_file='graph_temperature.png'):
    """Plot soil temperature over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(sorted(zones_data.items())):
        if len(zone_data['timestamps']) > 0 and zone_data['temp']:
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(zone_data['timestamps'], zone_data['temp'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='.',
                   markersize=2,
                   alpha=0.8)

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Soil Temperature (°C)', fontsize=12, fontweight='bold')
    ax.set_title('Soil Temperature Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_multi_metric_dashboard(zones_data, output_file='graph_dashboard.png'):
    """Create a multi-metric dashboard for a single zone (average of all zones)."""
    fig, axes = plt.subplots(3, 2, figsize=(14, 12))
    fig.suptitle('Multi-Metric Dashboard - All Zones Average', fontsize=16, fontweight='bold', y=0.995)

    # Calculate averages across all zones
    avg_data = defaultdict(list)
    all_times = []

    for zone_id, zone_data in zones_data.items():
        if not all_times:
            all_times = zone_data['timestamps']
        for key in ['vwc', 'psi', 'aw', 'depletion', 'confidence', 'drying_rate']:
            if not avg_data[key]:
                avg_data[key] = [0] * len(all_times)
            for i, val in enumerate(zone_data[key]):
                if i < len(avg_data[key]):
                    avg_data[key][i] += val

    # Average by number of zones
    num_zones = len(zones_data)
    for key in avg_data:
        avg_data[key] = [v / num_zones for v in avg_data[key]]

    # Plot 1: VWC
    axes[0, 0].plot(all_times, avg_data['vwc'], color=COLORS['healthy'], linewidth=2)
    axes[0, 0].axhline(y=15, color=COLORS['critical'], linestyle='--', alpha=0.5)
    axes[0, 0].axhline(y=25, color=COLORS['warning'], linestyle='--', alpha=0.5)
    axes[0, 0].set_ylabel('VWC (%)', fontweight='bold')
    axes[0, 0].set_title('Volumetric Water Content')
    axes[0, 0].grid(True, alpha=0.3)

    # Plot 2: Matric Potential
    axes[0, 1].plot(all_times, avg_data['psi'], color='#1976D2', linewidth=2)
    axes[0, 1].set_ylabel('Psi (kPa)', fontweight='bold')
    axes[0, 1].set_title('Matric Potential')
    axes[0, 1].grid(True, alpha=0.3)

    # Plot 3: Available Water
    axes[1, 0].plot(all_times, avg_data['aw'], color='#00796B', linewidth=2)
    axes[1, 0].set_ylabel('AW (mm)', fontweight='bold')
    axes[1, 0].set_title('Available Water')
    axes[1, 0].grid(True, alpha=0.3)

    # Plot 4: Depletion
    axes[1, 1].plot(all_times, avg_data['depletion'], color='#F57C00', linewidth=2)
    axes[1, 1].axhline(y=50, color=COLORS['warning'], linestyle='--', alpha=0.5)
    axes[1, 1].axhline(y=75, color=COLORS['critical'], linestyle='--', alpha=0.5)
    axes[1, 1].set_ylabel('Depletion (%)', fontweight='bold')
    axes[1, 1].set_title('Soil Water Depletion')
    axes[1, 1].set_ylim(0, 100)
    axes[1, 1].grid(True, alpha=0.3)

    # Plot 5: Confidence
    axes[2, 0].plot(all_times, avg_data['confidence'], color='#7B1FA2', linewidth=2)
    axes[2, 0].axhline(y=50, color=COLORS['warning'], linestyle='--', alpha=0.5)
    axes[2, 0].axhline(y=80, color=COLORS['healthy'], linestyle='--', alpha=0.5)
    axes[2, 0].set_ylabel('Confidence (%)', fontweight='bold')
    axes[2, 0].set_xlabel('Time (minutes)', fontweight='bold')
    axes[2, 0].set_title('Calibration Confidence')
    axes[2, 0].set_ylim(0, 100)
    axes[2, 0].grid(True, alpha=0.3)

    # Plot 6: Drying Rate
    filtered_dr = [(t, r * 100) for t, r in zip(all_times, avg_data['drying_rate']) if r != 0]
    if filtered_dr:
        dr_times, dr_vals = zip(*filtered_dr)
        axes[2, 1].plot(dr_times, dr_vals, color='#C62828', linewidth=2)
    axes[2, 1].axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    axes[2, 1].set_ylabel('Rate (% VWC/hr)', fontweight='bold')
    axes[2, 1].set_xlabel('Time (minutes)', fontweight='bold')
    axes[2, 1].set_title('Drying Rate')
    axes[2, 1].grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def generate_summary_report(data, zones_data, output_file='simulation_report.txt'):
    """Generate a text summary report of the simulation."""
    with open(output_file, 'w') as f:
        f.write("=" * 70 + "\n")
        f.write("AGRISCAN SIMULATION SUMMARY REPORT\n")
        f.write("=" * 70 + "\n\n")

        # Simulation overview
        if data:
            f.write(f"Total data points: {len(data)}\n")
            if zones_data:
                max_time = max([max(z['timestamps']) for z in zones_data.values() if z['timestamps']])
                f.write(f"Simulation duration: {max_time:.1f} minutes\n")

            # Count unique phases
            phases = set(entry.get('phase', '') for entry in data)
            phases.discard('')
            f.write(f"Phases: {', '.join(sorted(phases))}\n")

        f.write(f"\nActive zones: {len(zones_data)}\n")
        f.write("\n" + "-" * 70 + "\n")
        f.write("ZONE STATISTICS\n")
        f.write("-" * 70 + "\n\n")

        # Zone statistics
        for zone_id, zone_data in sorted(zones_data.items()):
            if len(zone_data['vwc']) > 0:
                f.write(f"Zone {zone_id}:\n")
                f.write(f"  VWC: min={min(zone_data['vwc']):.1f}%, "
                       f"max={max(zone_data['vwc']):.1f}%, "
                       f"avg={np.mean(zone_data['vwc']):.1f}%\n")
                f.write(f"  Matric Potential: min={min(zone_data['psi']):.1f} kPa, "
                       f"max={max(zone_data['psi']):.1f} kPa\n")
                f.write(f"  Available Water: min={min(zone_data['aw']):.1f} mm, "
                       f"max={max(zone_data['aw']):.1f} mm\n")
                f.write(f"  Depletion: min={min(zone_data['depletion']):.1f}%, "
                       f"max={max(zone_data['depletion']):.1f}%\n")
                f.write(f"  Raw ADC: min={min(zone_data['raw'])}, "
                       f"max={max(zone_data['raw'])}\n")

                # Confidence stats
                if zone_data['confidence']:
                    f.write(f"  Confidence: min={min(zone_data['confidence']):.1f}%, "
                           f"max={max(zone_data['confidence']):.1f}%, "
                           f"avg={np.mean(zone_data['confidence']):.1f}%\n")

                # Drying rate stats (filter out zeros)
                non_zero_dr = [dr for dr in zone_data['drying_rate'] if dr != 0]
                if non_zero_dr:
                    f.write(f"  Drying Rate: min={min(non_zero_dr)*100:.2f}%/hr, "
                           f"max={max(non_zero_dr)*100:.2f}%/hr, "
                           f"avg={np.mean(non_zero_dr)*100:.2f}%/hr\n")

                # Status distribution
                status_count = {}
                for status in zone_data['status']:
                    status_count[status] = status_count.get(status, 0) + 1
                f.write(f"  Status distribution: {dict(status_count)}\n")

                # Regime distribution
                regime_count = {}
                for regime in zone_data['regime']:
                    regime_count[regime] = regime_count.get(regime, 0) + 1
                f.write(f"  Regime distribution: {dict(regime_count)}\n")
                f.write("\n")

        f.write("=" * 70 + "\n")
        f.write("End of Report\n")
        f.write("=" * 70 + "\n")

    print(f"📄 Saved: {output_file}")

def main():
    """Main visualization function."""
    print("\n" + "=" * 70)
    print("AgriScan Simulation Data Visualizer")
    print("=" * 70 + "\n")

    # Create session directory
    session_dir = create_session_directory()
    print(f"📁 Session directory: {session_dir}\n")

    # Determine input file
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    else:
        input_file = 'simulation_logs.json'

    # Load data
    data = load_simulation_data(input_file)

    # Extract zone data
    zones_data = extract_zone_data(data)

    if not zones_data:
        print("❌ No zone data found in simulation logs")
        sys.exit(1)

    print(f"📍 Found {len(zones_data)} active zones: {', '.join(sorted(zones_data.keys()))}")
    print(f"🔄 Generating visualizations...\n")

    # Generate all plots
    plot_vwc_over_time(zones_data, session_dir / 'graph_vwc.png')
    plot_matric_potential(zones_data, session_dir / 'graph_psi.png')
    plot_available_water(zones_data, session_dir / 'graph_aw.png')
    plot_depletion_percentage(zones_data, session_dir / 'graph_depletion.png')
    plot_status_distribution(data, session_dir / 'graph_status.png')
    plot_phase_timeline(data, zones_data, session_dir / 'graph_phases.png')
    plot_raw_adc_values(zones_data, session_dir / 'graph_raw.png')

    # New metric plots
    plot_confidence_over_time(zones_data, session_dir / 'graph_confidence.png')
    plot_drying_rate(zones_data, session_dir / 'graph_drying_rate.png')
    plot_temperature(zones_data, session_dir / 'graph_temperature.png')
    plot_multi_metric_dashboard(zones_data, session_dir / 'graph_dashboard.png')

    # Generate summary report
    generate_summary_report(data, zones_data, session_dir / 'simulation_report.txt')

    print("\n✅ All visualizations complete!")
    print(f"\n📂 All files saved to: {session_dir}")
    print("\nGenerated files:")
    print("  1. graph_vwc.png - Volumetric Water Content")
    print("  2. graph_psi.png - Matric Potential")
    print("  3. graph_aw.png - Available Water")
    print("  4. graph_depletion.png - Depletion Percentage")
    print("  5. graph_status.png - Status Distribution")
    print("  6. graph_phases.png - Phase Timeline")
    print("  7. graph_raw.png - Raw ADC Sensor Readings")
    print("  8. graph_confidence.png - Calibration Confidence ⭐ NEW")
    print("  9. graph_drying_rate.png - Soil Moisture Change Rate ⭐ NEW")
    print(" 10. graph_temperature.png - Soil Temperature ⭐ NEW")
    print(" 11. graph_dashboard.png - Multi-Metric Dashboard ⭐ NEW")
    print(" 12. simulation_report.txt - Summary Report")
    print("\n" + "=" * 70 + "\n")

if __name__ == '__main__':
    main()
