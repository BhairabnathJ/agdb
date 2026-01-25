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

# Color scheme matching AgriScan dashboard
COLORS = {
    'healthy': '#2E7D32',
    'warning': '#F9A825',
    'critical': '#C62828',
    'neutral': '#666666',
    'zones': ['#1976D2', '#388E3C', '#D32F2F', '#F57C00', '#7B1FA2',
              '#C2185B', '#00796B', '#5D4037', '#455A64']
}

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

def parse_timestamps(data):
    """Convert timestamps to datetime objects."""
    timestamps = []
    for entry in data:
        if 'timestamp' in entry:
            # ISO format timestamp
            timestamps.append(datetime.fromisoformat(entry['timestamp'].replace('Z', '+00:00')))
        elif 'elapsed_ms' in entry:
            # Relative time from start
            timestamps.append(timedelta(milliseconds=entry['elapsed_ms']))
    return timestamps

def extract_zone_data(data):
    """Extract data organized by zone."""
    zones_data = defaultdict(lambda: {
        'timestamps': [],
        'vwc': [],
        'psi': [],
        'aw': [],
        'depletion': [],
        'status': [],
        'regime': []
    })

    for entry in data:
        if 'zones' in entry:
            for zone_id, zone_data in entry['zones'].items():
                if zone_data.get('active', False):
                    zones_data[zone_id]['timestamps'].append(entry.get('elapsed_ms', 0))
                    zones_data[zone_id]['vwc'].append(zone_data.get('theta', 0) * 100)
                    zones_data[zone_id]['psi'].append(zone_data.get('psi_kPa', 0))
                    zones_data[zone_id]['aw'].append(zone_data.get('AW_mm', 0))
                    zones_data[zone_id]['depletion'].append(zone_data.get('fractionDepleted', 0) * 100)
                    zones_data[zone_id]['status'].append(zone_data.get('status', 'unknown'))
                    zones_data[zone_id]['regime'].append(zone_data.get('regime', 'unknown'))

    return zones_data

def plot_vwc_over_time(zones_data, output_file='graph_vwc.png'):
    """Plot Volumetric Water Content over time for all zones."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(zones_data.items()):
        if len(zone_data['timestamps']) > 0:
            # Convert ms to minutes
            time_minutes = [t / (60 * 1000) for t in zone_data['timestamps']]
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(time_minutes, zone_data['vwc'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='o',
                   markersize=3,
                   alpha=0.8)

    # Reference lines
    ax.axhline(y=35, color=COLORS['critical'], linestyle='--', alpha=0.5, label='Critical threshold')
    ax.axhline(y=45, color=COLORS['warning'], linestyle='--', alpha=0.5, label='Warning threshold')

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Volumetric Water Content (%)', fontsize=12, fontweight='bold')
    ax.set_title('VWC Over Time - All Zones', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_matric_potential(zones_data, output_file='graph_psi.png'):
    """Plot matric potential (soil water tension) over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(zones_data.items()):
        if len(zone_data['timestamps']) > 0:
            time_minutes = [t / (60 * 1000) for t in zone_data['timestamps']]
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(time_minutes, zone_data['psi'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='s',
                   markersize=3,
                   alpha=0.8)

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Matric Potential (kPa)', fontsize=12, fontweight='bold')
    ax.set_title('Soil Water Tension Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)

    # Invert y-axis (more negative = drier soil)
    ax.invert_yaxis()

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_available_water(zones_data, output_file='graph_aw.png'):
    """Plot available water depth over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(zones_data.items()):
        if len(zone_data['timestamps']) > 0:
            time_minutes = [t / (60 * 1000) for t in zone_data['timestamps']]
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(time_minutes, zone_data['aw'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='^',
                   markersize=3,
                   alpha=0.8)

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Available Water (mm)', fontsize=12, fontweight='bold')
    ax.set_title('Available Water Depth Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_depletion_percentage(zones_data, output_file='graph_depletion.png'):
    """Plot soil water depletion percentage over time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, (zone_id, zone_data) in enumerate(zones_data.items()):
        if len(zone_data['timestamps']) > 0:
            time_minutes = [t / (60 * 1000) for t in zone_data['timestamps']]
            color = COLORS['zones'][idx % len(COLORS['zones'])]
            ax.plot(time_minutes, zone_data['depletion'],
                   label=f'Zone {zone_id}',
                   linewidth=2,
                   color=color,
                   marker='d',
                   markersize=3,
                   alpha=0.8)

    # Reference lines
    ax.axhline(y=50, color=COLORS['warning'], linestyle='--', alpha=0.5, label='MAD threshold (50%)')
    ax.axhline(y=75, color=COLORS['critical'], linestyle='--', alpha=0.5, label='Critical depletion')

    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Depletion (%)', fontsize=12, fontweight='bold')
    ax.set_title('Soil Water Depletion Over Time', fontsize=14, fontweight='bold', pad=20)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 100)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"📊 Saved: {output_file}")
    plt.close()

def plot_status_distribution(data, output_file='graph_status.png'):
    """Plot distribution of status categories over time."""
    # Count status occurrences at each time point
    time_points = []
    status_counts = defaultdict(lambda: {'healthy': 0, 'warning': 0, 'critical': 0, 'unknown': 0})

    for entry in data:
        if 'zones' in entry:
            time_min = entry.get('elapsed_ms', 0) / (60 * 1000)
            time_points.append(time_min)

            for zone_id, zone_data in entry['zones'].items():
                if zone_data.get('active', False):
                    status = zone_data.get('status', 'unknown')
                    status_counts[time_min][status] += 1

    if not time_points:
        print("⚠️  No status data available for plotting")
        return

    time_points = sorted(set(time_points))
    healthy_counts = [status_counts[t]['healthy'] for t in time_points]
    warning_counts = [status_counts[t]['warning'] for t in time_points]
    critical_counts = [status_counts[t]['critical'] for t in time_points]

    fig, ax = plt.subplots(figsize=(12, 6))

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

def plot_phase_timeline(data, output_file='graph_phases.png'):
    """Plot simulation phases over time."""
    phases = []
    phase_times = []

    for entry in data:
        if 'phase' in entry:
            phases.append(entry['phase'])
            phase_times.append(entry.get('elapsed_ms', 0) / (60 * 1000))

    if not phases:
        print("⚠️  No phase data available for plotting")
        return

    # Create phase visualization
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), height_ratios=[1, 3])

    # Phase timeline
    unique_phases = []
    phase_starts = []
    phase_colors_map = {}
    current_phase = None

    for i, phase in enumerate(phases):
        if phase != current_phase:
            unique_phases.append(phase)
            phase_starts.append(phase_times[i])
            current_phase = phase
            # Assign color based on phase name
            if 'irrigation' in phase.lower() or 'wetting' in phase.lower():
                phase_colors_map[phase] = COLORS['healthy']
            elif 'drying' in phase.lower() or 'drought' in phase.lower():
                phase_colors_map[phase] = COLORS['critical']
            else:
                phase_colors_map[phase] = COLORS['neutral']

    # Add end time
    phase_starts.append(phase_times[-1])

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
        ax1.text(mid, 0, unique_phases[i].replace('_', ' ').title(),
                ha='center', va='center', fontsize=9, fontweight='bold')

    ax1.set_xlim(0, phase_times[-1])
    ax1.set_ylim(-0.5, 0.5)
    ax1.set_yticks([])
    ax1.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax1.set_title('Simulation Phase Timeline', fontsize=14, fontweight='bold', pad=20)
    ax1.grid(True, axis='x', alpha=0.3)

    # Average VWC during each phase
    phase_avg_vwc = defaultdict(list)
    for entry in data:
        if 'phase' in entry and 'zones' in entry:
            phase_name = entry['phase']
            for zone_id, zone_data in entry['zones'].items():
                if zone_data.get('active', False):
                    vwc = zone_data.get('theta', 0) * 100
                    phase_avg_vwc[phase_name].append(vwc)

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
        ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                f'{val:.1f}%', ha='center', va='bottom', fontsize=9, fontweight='bold')

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
            if 'elapsed_ms' in data[-1]:
                duration_min = data[-1]['elapsed_ms'] / (60 * 1000)
                f.write(f"Simulation duration: {duration_min:.1f} minutes\n")
            if 'simulation' in data[0]:
                f.write(f"Simulation type: {data[0]['simulation']}\n")

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

                # Status distribution
                status_count = {}
                for status in zone_data['status']:
                    status_count[status] = status_count.get(status, 0) + 1
                f.write(f"  Status distribution: {dict(status_count)}\n")
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

    print(f"📍 Found {len(zones_data)} active zones")
    print(f"🔄 Generating visualizations...\n")

    # Generate all plots
    plot_vwc_over_time(zones_data)
    plot_matric_potential(zones_data)
    plot_available_water(zones_data)
    plot_depletion_percentage(zones_data)
    plot_status_distribution(data)
    plot_phase_timeline(data)

    # Generate summary report
    generate_summary_report(data, zones_data)

    print("\n✅ All visualizations complete!")
    print("\nGenerated files:")
    print("  - graph_vwc.png (Volumetric Water Content)")
    print("  - graph_psi.png (Matric Potential)")
    print("  - graph_aw.png (Available Water)")
    print("  - graph_depletion.png (Depletion Percentage)")
    print("  - graph_status.png (Status Distribution)")
    print("  - graph_phases.png (Phase Timeline)")
    print("  - simulation_report.txt (Summary Report)")
    print("\n" + "=" * 70 + "\n")

if __name__ == '__main__':
    main()
