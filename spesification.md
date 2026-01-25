# **DOCUMENT 2: AgriScan Dashboard - Complete Website Specification**

---

## **1. Project Overview**

### **1.1 Design Philosophy**
**Core Principle:** *"Clarity over cleverness, function over flash"*

AgriScan serves farmers earning <$200/month in remote areas with basic phones and limited technical literacy. Every design decision prioritizes:
- **Immediate comprehension** (no learning curve)
- **Offline reliability** (works without internet)
- **Low bandwidth** (minimal file size)
- **Accessibility** (works on 320px screens to desktop)
- **Trust** (professional, credible, stable)

### **1.2 Technical Constraints**
- **Platform:** ESP32 microcontroller serving static files
- **Storage:** SPIFFS/LittleFS filesystem (max 500KB total)
- **Languages:** Vanilla HTML5, CSS3, JavaScript (ES6)
- **No frameworks:** No React, Vue, Bootstrap - pure code only
- **No external dependencies:** No CDNs, no Google Fonts, no icon libraries
- **Offline-first:** All assets bundled, works without internet

---

## **2. Sitemap & Information Architecture**

### **2.1 Site Structure (Phase-by-Phase)**

```
AgriScan Dashboard (Single-Page Application)
│
├── Phase 0: MVP
│   └── 01_Live View (Single Page)
│       ├── Current moisture reading
│       ├── Current temperature reading
│       ├── LED status indicator
│       └── Last update timestamp
│
├── Phase 1: Decision Support
│   ├── 01_Overview (Enhanced)
│   │   ├── Global status strip
│   │   ├── Live metrics grid (4-5 values)
│   │   ├── 24h mini-chart
│   │   └── Priority action card
│   │
│   └── 02_History
│       ├── Time range selector (24h/7d/30d)
│       ├── Metric selector (moisture/temp)
│       ├── Line chart
│       └── Summary statistics
│
├── Phase 2: Multi-Zone
│   ├── 01_Overview (Multi-zone aware)
│   │   ├── Global status (worst zone)
│   │   └── Top 3 issues card
│   │
│   ├── 02_Map (NEW)
│   │   ├── Zone grid (dynamic 4x4 or 8x8)
│   │   ├── Zone drawer (bottom sheet)
│   │   ├── Battery indicators
│   │   └── Color legend
│   │
│   └── 03_History (Zone filtering)
│       ├── Zone dropdown filter
│       └── Multi-zone overlay toggle
│
└── Phase 3: Full Platform
    ├── 01_Overview (Intelligence layer)
    │   ├── Irrigation recommendations
    │   ├── Weather alerts (if connected)
    │   └── Disease risk indicators
    │
    ├── 02_Financial (NEW)
    │   ├── ROI tracker
    │   ├── Water savings calculator
    │   ├── Season comparison charts
    │   └── Yield estimation
    │
    ├── 03_Map (Enhanced)
    │   ├── EC overlay toggle
    │   ├── Smart sorting options
    │   └── Sensor ping functionality
    │
    ├── 04_History (Predictive)
    │   ├── Forecast overlay
    │   ├── Event annotations
    │   └── Export options (CSV/PDF)
    │
    └── 05_Settings (NEW)
        ├── Crop profile setup
        ├── Calibration helper
        ├── Alert preferences
        └── System diagnostics
```

### **2.2 Navigation Structure**

**Phase 0:** No navigation (single page)

**Phase 1:**
```
[Overview] [History]
```

**Phase 2:**
```
[Overview] [Map] [History]
```

**Phase 3:**
```
[Overview] [Financial] [Map] [History] [⚙️]
```

**Navigation Type:** Fixed bottom tab bar (thumb-zone optimized)

---

## **3. Visual Design System**

### **3.1 Color Palette**

#### **Primary Colors (Functional Status)**
```css
/* Status Colors - Core System */
--status-critical: #C62828;    /* Red - Immediate action required */
--status-warning: #F9A825;     /* Yellow - Monitor closely */
--status-healthy: #2E7D32;     /* Green - Optimal conditions */
--status-offline: #9E9E9E;     /* Gray - Stale data / offline */
```

**Usage Rules:**
- **Critical (Red):** Moisture <30% OR rapid drying (>5%/hour) OR battery <10%
- **Warning (Yellow):** Moisture 30-50% OR gradual drying (2-5%/hour) OR battery 10-25%
- **Healthy (Green):** Moisture >50% AND stable/increasing AND battery >25%
- **Offline (Gray):** No data for >30 minutes

#### **Neutral Colors (UI Structure)**
```css
/* Neutrals - Interface Elements */
--neutral-900: #111111;        /* Primary text, active icons */
--neutral-700: #424242;        /* Secondary headings */
--neutral-600: #666666;        /* Body text, inactive states */
--neutral-400: #9E9E9E;        /* Disabled text, placeholders */
--neutral-300: #E0E0E0;        /* Borders, dividers */
--neutral-200: #EEEEEE;        /* Hover states */
--neutral-100: #F7F7F7;        /* Page background, card fills */
--neutral-50: #FAFAFA;         /* Subtle backgrounds */
--white: #FFFFFF;              /* Pure white for cards */
```

**Usage Rules:**
- 900: Headlines, primary data values, active tab labels
- 600: Body text, labels, descriptions
- 300: All borders (1px solid)
- 100: Page background color
- White: Elevated cards, modals, overlays

#### **Accent Colors (Optional/Rare Use)**
```css
/* Accents - Use Sparingly */
--accent-blue: #1976D2;        /* Links, info messages (if needed) */
--accent-purple: #7B1FA2;      /* Premium features (future) */
```

**Usage:** Only for interactive elements like links or special badges. Do NOT use for status indicators.

#### **Chart Colors (Data Visualization)**
```css
/* Chart Lines & Fills */
--chart-primary: #2E7D32;      /* Main line (moisture/temp) */
--chart-secondary: #1976D2;    /* Secondary line (VPD/comparison) */
--chart-fill: rgba(46, 125, 50, 0.1);  /* Healthy zone shading */
--chart-grid: #E0E0E0;         /* Gridlines */
--chart-axis: #666666;         /* Axis labels */
```

---

### **3.2 Typography**

#### **Font Family (System Native - Zero Load Time)**
```css
font-family: -apple-system, BlinkMacSystemFont, 
             "Segoe UI", Roboto, Oxygen-Sans, Ubuntu, 
             Cantarell, "Helvetica Neue", sans-serif;
```

**Why System Fonts:**
- Zero network requests
- Native rendering quality
- Instant load times
- Accessibility (respects system font size settings)

#### **Type Scale**
```css
/* Display - Hero Numbers */
--font-display: 48px;          /* 3rem - Large sensor readings */
--font-display-weight: 700;

/* H1 - Page Titles */
--font-h1: 24px;              /* 1.5rem - Global status text */
--font-h1-weight: 700;

/* H2 - Section Headers */
--font-h2: 18px;              /* 1.125rem - Card titles */
--font-h2-weight: 600;

/* H3 - Sub-headers */
--font-h3: 16px;              /* 1rem - Zone labels, metric names */
--font-h3-weight: 600;

/* Body - Primary Text */
--font-body: 14px;            /* 0.875rem - Default text */
--font-body-weight: 400;

/* Small - Metadata */
--font-small: 12px;           /* 0.75rem - Timestamps, captions */
--font-small-weight: 400;

/* Caption - Tiny Labels */
--font-caption: 11px;         /* 0.6875rem - Chart axes, fine print */
--font-caption-weight: 400;
```

#### **Line Heights**
```css
--line-height-tight: 1.2;     /* Headlines, data values */
--line-height-normal: 1.5;    /* Body text */
--line-height-relaxed: 1.7;   /* Long-form content (if any) */
```

#### **Font Usage Matrix**

| Element | Size | Weight | Line Height | Color |
|---------|------|--------|-------------|-------|
| Global Status Text | 24px | 700 | 1.2 | White (on color bg) |
| Sensor Reading Value | 48px | 700 | 1.0 | Neutral-900 |
| Metric Label | 12px | 400 | 1.2 | Neutral-600 |
| Card Title | 18px | 600 | 1.2 | Neutral-900 |
| Action Card Message | 16px | 600 | 1.5 | Neutral-900 |
| Body Text | 14px | 400 | 1.5 | Neutral-600 |
| Timestamp | 12px | 400 | 1.2 | Neutral-600 |
| Button Text | 14px | 600 | 1.2 | White/Neutral-900 |
| Tab Label | 12px | 600 | 1.2 | Neutral-600/900 |

---

### **3.3 Spacing System (8px Grid)**

```css
/* Spacing Scale - Based on 8px Grid */
--space-xs: 4px;      /* 0.25rem - Tight padding */
--space-sm: 8px;      /* 0.5rem - Icon margins, small gaps */
--space-md: 16px;     /* 1rem - Default padding, standard gaps */
--space-lg: 24px;     /* 1.5rem - Section spacing */
--space-xl: 32px;     /* 2rem - Page margins */
--space-2xl: 48px;    /* 3rem - Major section breaks */
```

**Usage Guidelines:**
- **Card padding:** 16px (md)
- **Page margins:** 16px mobile, 24px tablet+
- **Section gaps:** 24px between major sections
- **Button padding:** 12px vertical, 24px horizontal
- **Input padding:** 12px all sides

---

### **3.4 Component Specifications**

#### **3.4.1 Top Bar (Fixed Header)**
```
Height: 56px
Background: White
Border-bottom: 1px solid Neutral-300
Z-index: 100

Layout:
┌────────────────────────────────────┐
│ ■ AGRISCAN    [●] 2 min ago    ≡  │
│ (Logo/Name)   (Status) (Time) (Menu)│
└────────────────────────────────────┘

Components:
- Logo/Brand (Left): 24px height, Neutral-900
- Hub Status LED (Center-Right): 8px circle, Green/Red/Gray
- Last Update Text (Center-Right): 12px, Neutral-600
- Menu Icon (Right): 24px, Neutral-900 (Phase 3+)
```

#### **3.4.2 Bottom Tab Bar (Fixed Navigation)**
```
Height: 64px
Background: White
Border-top: 1px solid Neutral-300
Z-index: 100

Layout (Phase 1):
┌────────────────────────────────────┐
│   [Overview]      [History]        │
│      (Icon)         (Icon)         │
│      Label          Label          │
└────────────────────────────────────┘

Tab States:
Active:
  - Icon color: Status-Healthy (Green)
  - Label color: Neutral-900
  - Label weight: 600
  - Top border: 2px solid Status-Healthy

Inactive:
  - Icon color: Neutral-600
  - Label color: Neutral-600
  - Label weight: 400

Tap Target: 48px minimum height
```

**Icons (Inline SVG):**
```html
<!-- Overview Icon (Home) -->
<svg width="24" height="24" viewBox="0 0 24 24">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/>
</svg>

<!-- Map Icon (Grid) -->
<svg width="24" height="24" viewBox="0 0 24 24">
  <path d="M3 3h8v8H3zm10 0h8v8h-8zM3 13h8v8H3zm10 0h8v8h-8z"/>
</svg>

<!-- History Icon (Chart) -->
<svg width="24" height="24" viewBox="0 0 24 24">
  <path d="M3 13h2v8H3zm4-4h2v12H7zm4-4h2v16h-2zm4 2h2v14h-2zm4-6h2v20h-2z"/>
</svg>
```

#### **3.4.3 Global Status Strip (Phase 1+)**
```
Height: 60px
Full width
Text: Centered, H1 (24px bold), White
Background: Status color (Critical/Warning/Healthy)

Example:
┌────────────────────────────────────┐
│                                    │
│     ALL ZONES HEALTHY              │ ← Green bg
│                                    │
└────────────────────────────────────┘

Or:
┌────────────────────────────────────┐
│                                    │
│  ⚠️ CRITICAL ACTION NEEDED          │ ← Red bg
│                                    │
└────────────────────────────────────┘
```

#### **3.4.4 Metric Card (Live Data Display)**
```
Dimensions: Flexible, 2-column grid on mobile
Padding: 16px
Background: White
Border: 1px solid Neutral-300
Border-radius: 8px

Layout:
┌──────────────┐
│ Moisture     │ ← Label: 12px, Neutral-600
│              │
│    45%       │ ← Value: 48px bold, Neutral-900
│    ↓↓        │ ← Trend: 24px, Status color
│              │
└──────────────┘

Trend Indicators:
↑↑ Rapid increase (Green)
↑  Gradual increase (Green)
→  Stable (Gray)
↓  Gradual decrease (Yellow)
↓↓ Rapid decrease (Red)
```

#### **3.4.5 Action Card (Priority Alert)**
```
Width: Full width - 32px (page margins)
Padding: 16px
Background: White
Border-left: 4px solid Status-color
Border-radius: 8px
Box-shadow: 0 2px 8px rgba(0,0,0,0.08)

Layout:
┌────────────────────────────────────┐
│ 🔴 IRRIGATE ZONE B3                │ ← Title: 16px bold
│                                    │
│ Critical in 3.2 hours              │ ← Subtitle: 14px
│ Apply 0.5" over 2 hours            │    Neutral-600
│                                    │
│ [View Details]                     │ ← Button (optional)
└────────────────────────────────────┘

Badge Icons (Unicode):
🔴 Critical (Red)
🟡 Warning (Yellow)
🟢 Monitor (Green)
```

#### **3.4.6 Chart Container (24h Trend)**
```
Height: 200px
Padding: 16px
Background: White
Border: 1px solid Neutral-300
Border-radius: 8px

Chart Elements:
- SVG viewBox: "0 0 400 180" (scales to container)
- Line: 2px stroke, Chart-Primary color
- Fill: Chart-Fill (0.1 opacity green)
- Grid: 1px, Chart-Grid color
- Axis labels: 11px, Chart-Axis color
- Healthy zone band: 50-70%, light green fill

No chart library - pure SVG path drawing:
<path d="M 0,100 L 50,90 L 100,85..." 
      stroke="#2E7D32" 
      stroke-width="2" 
      fill="none"/>
```

#### **3.4.7 Zone Grid Cell (Phase 2)**
```
Size: 40px × 40px (mobile), 60px × 60px (tablet+)
Border: 1px solid Neutral-300
Border-radius: 4px
Background: Status color
Text: Zone ID (e.g., "B3"), 12px bold, White

Grid Layout:
display: grid;
grid-template-columns: repeat(auto-fit, minmax(40px, 1fr));
gap: 4px;

Cell States:
Normal: Status color background
Active/Selected: 2px solid Neutral-900 border
Stale (>30min): Grayscale filter + diagonal stripes
Low Battery: Yellow corner badge (⚠️)
```

#### **3.4.8 Zone Drawer (Bottom Sheet)**
```
Height: Auto (max 60vh)
Background: White
Border-radius: 16px 16px 0 0
Box-shadow: 0 -4px 16px rgba(0,0,0,0.12)
Animation: Slide up (200ms ease-out)

Layout:
┌────────────────────────────────────┐
│ ╌╌╌╌ (Drag handle)                 │ ← 32px tall
├────────────────────────────────────┤
│ Zone B3                            │ ← H2 title
│ Sensor ID: 0x4F2A                  │ ← Small text
│                                    │
│ Battery: 87% [████████░░]          │ ← Progress bar
│                                    │
│ Moisture: 28% (LOW)                │ ← Status badge
│ Temperature: 24°C                  │
│ Last Reading: 1 min ago            │
│                                    │
│ [Ping Sensor] [View History]       │ ← Action buttons
└────────────────────────────────────┘

Overlay: rgba(0,0,0,0.4) behind drawer
Close: Tap overlay or drag down
```

#### **3.4.9 Button Styles**

**Primary Button (Action)**
```css
Padding: 12px 24px
Background: Status-Healthy (Green)
Color: White
Border: None
Border-radius: 8px
Font: 14px, 600
Min-height: 44px (touch target)

Hover: Darken 10%
Active: Darken 20%, scale(0.98)
Disabled: Neutral-300 bg, Neutral-600 text
```

**Secondary Button (Cancel/Alternative)**
```css
Padding: 12px 24px
Background: White
Color: Neutral-900
Border: 1px solid Neutral-300
Border-radius: 8px
Font: 14px, 600

Hover: Neutral-100 background
Active: Neutral-200 background
```

**Ghost Button (Tertiary)**
```css
Padding: 8px 16px
Background: Transparent
Color: Status-Healthy
Border: None
Font: 14px, 600

Hover: Underline
Active: Neutral-100 background
```

---

## **4. Page-Specific Layouts**

### **4.1 Phase 0: Live View (Single Page)**

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#2E7D32">
  <title>AgriScan</title>
  <style>
    /* Inline CSS - see Section 5 */
  </style>
</head>
<body>
  <div class="container">
    <!-- Status LED Indicator -->
    <div class="led-indicator">
      <div class="led-circle led-green"></div>
      <p class="led-label">System Online</p>
    </div>

    <!-- Live Readings -->
    <div class="metrics-grid">
      <div class="metric-card">
        <p class="metric-label">Soil Moisture</p>
        <p class="metric-value" id="moisture">--</p>
        <p class="metric-unit">%</p>
      </div>

      <div class="metric-card">
        <p class="metric-label">Soil Temperature</p>
        <p class="metric-value" id="temperature">--</p>
        <p class="metric-unit">°C</p>
      </div>
    </div>

    <!-- Physical LED Status -->
    <div class="led-status">
      <div class="led-physical led-green"></div>
      <p class="led-status-text">Optimal Conditions</p>
    </div>

    <!-- Timestamp -->
    <p class="timestamp" id="timestamp">Last updated: --</p>
  </div>

  <script>
    // JavaScript - see Section 6
  </script>
</body>
</html>
```

**Visual Layout:**
```
┌────────────────────────────────────┐
│                                    │
│         [●] System Online          │ ← Top indicator
│                                    │
├────────────────────────────────────┤
│                                    │
│  ┌──────────┐  ┌──────────┐       │
│  │ Moisture │  │   Temp   │       │ ← Metric cards
│  │   45%    │  │   22°C   │       │   (2-column grid)
│  └──────────┘  └──────────┘       │
│                                    │
├────────────────────────────────────┤
│         🟢 Optimal                 │ ← Physical LED
│         Conditions                 │   status mirror
│                                    │
├────────────────────────────────────┤
│    Last updated: 2 seconds ago     │ ← Timestamp
│                                    │
└────────────────────────────────────┘
```

---

### **4.2 Phase 1: Overview + History**

#### **Overview Page Layout:**
```
┌────────────────────────────────────┐
│ ■ AGRISCAN         [●] 2 min ago   │ ← Top bar (56px)
├────────────────────────────────────┤
│                                    │
│     ALL ZONES HEALTHY              │ ← Status strip (60px)
│                                    │
├────────────────────────────────────┤
│ Live Metrics                       │ ← Section header
│                                    │
│  ┌────────┐ ┌────────┐ ┌────────┐ │
│  │ 45%    │ │ 22°C   │ │ 65%    │ │ ← 3-column grid
│  │Moisture│ │SoilTemp│ │AirHumid│ │   (wraps on mobile)
│  │  ↓     │ │  ↑     │ │  →     │ │
│  └────────┘ └────────┘ └────────┘ │
│                                    │
│  ┌────────┐ ┌────────┐            │
│  │ 24°C   │ │ 1.2kPa │            │ ← 2nd row
│  │AirTemp │ │  VPD   │            │
│  │  ↑     │ │  →     │            │
│  └────────┘ └────────┘            │
│                                    │
├────────────────────────────────────┤
│ 24h Trend                          │
│ ┌────────────────────────────────┐ │
│ │     /\      /\       /\        │ │ ← Chart (200px)
│ │    /  \    /  \     /  \       │ │
│ │   /    \__/    \___/    \__    │ │
│ │ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ │ │ ← Healthy zone
│ └────────────────────────────────┘ │
│                                    │
├────────────────────────────────────┤
│ Priority Actions                   │
│ ┌────────────────────────────────┐ │
│ │ 🟢 All Optimal                 │ │ ← Action card
│ │ No action needed               │ │
│ │                                │ │
│ │ Next check: In 4 hours         │ │
│ └────────────────────────────────┘ │
│                                    │
└────────────────────────────────────┘
│   [Overview]           [History]   │ ← Bottom tabs (64px)
└────────────────────────────────────┘
```

#### **History Page Layout:**
```
┌────────────────────────────────────┐
│ ■ AGRISCAN         [●] 2 min ago   │
├────────────────────────────────────┤
│                                    │
│  [24h] [7d] [30d]   [💧] [🌡]     │ ← Toggle buttons
│                                    │
├────────────────────────────────────┤
│ Moisture (Last 24 Hours)           │
│                                    │
│ ┌────────────────────────────────┐ │
│ │ 70%                            │ │
│ │     /\      /\       /\        │ │ ← Chart (300px)
│ │    /  \    /  \     /  \       │ │
│ │   /    \__/    \___/    \__    │ │
│ │ 30%                            │ │
│ │ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ │ │
│ │ 12am   6am   12pm  6pm   12am  │ │
│ └────────────────────────────────┘ │
│                                    │
├────────────────────────────────────┤
│ Summary Statistics                 │
│                                    │
│  High: 68%  │  Low: 42%  │ Avg: 55%│
│                                    │
└────────────────────────────────────┘
│   [Overview]           [History]   │
└────────────────────────────────────┘
```

---

### **4.3 Phase 2: Multi-Zone with Map**

#### **Map Page Layout:**
```
┌────────────────────────────────────┐
│ ■ AGRISCAN         [●] 2 min ago   │
├────────────────────────────────────┤
│                                    │
│  Field Zones (8 Sensors Active)    │
│                                    │
│  ┌─┬─┬─┬─┬─┬─┬─┬─┐                │
│  │G│G│Y│G│G│G│G│G│                │ ← 8x8 Grid
│  ├─┼─┼─┼─┼─┼─┼─┼─┤                │   40x40px cells
│  │G│G│G│R│G│G│Y│G│                │   (tappable)
│  ├─┼─┼─┼─┼─┼─┼─┼─┤                │
│  │G│G│G│G│G│G│G│G│                │
│  ├─┼─┼─┼─┼─┼─┼─┼─┤                │
│  │ │ │ │ │ │ │ │ │                │ ← Empty cells
│  └─┴─┴─┴─┴─┴─┴─┴─┘                │   (no sensor)
│                                    │
│  Legend:                           │
│  🟢 Optimal  🟡 Warning  🔴 Critical│
│                                    │
├────────────────────────────────────┤
│ Top Issues:                        │
│ 1. Zone B3 (28%, ↓↓) ⚠️ LOW       │ ← Priority list
│ 2. Zone A5 (35%, ↓)               │
│ 3. Zone D7 (Battery 12%)          │
│                                    │
└────────────────────────────────────┘
│  [Overview] [Map] [History]        │
└────────────────────────────────────┘

When zone tapped, drawer slides up:

┌────────────────────────────────────┐
│                                    │
│         ╌╌╌╌  (drag handle)        │ ← Zone Drawer
├────────────────────────────────────┤   (overlay)
│ Zone B3                            │
│ Sensor ID: 0x4F2A                  │
│                                    │
│ Battery: 87% [████████░░]          │
│                                    │
│ Moisture: 28% 🔴 CRITICAL          │
│ Soil Temp: 24°C                    │
│ Air Temp: 26°C                     │
│ Humidity: 65%                      │
│ VPD: 1.4 kPa                       │
│                                    │
│ Last Reading: 1 min ago            │
│                                    │
│ [Ping Sensor] [View History]       │
└────────────────────────────────────┘
```

---

### **4.4 Phase 3: Full Platform**

#### **Financial Dashboard (New Page):**
```
┌────────────────────────────────────┐
│ ■ AGRISCAN         [●] 2 min ago   │
├────────────────────────────────────┤
│                                    │
│  Season Performance                │
│                                    │
│  ┌────────────────────────────────┐│
│  │ ROI Summary                    ││ ← Summary card
│  │                                ││
│  │ Device Cost: $150              ││
│  │ Water Saved: $45               ││
│  │ Yield Increase: $180 (est.)    ││
│  │                                ││
│  │ Net Benefit: $75               ││
│  │ Payback: Week 8 ✓              ││
│  └────────────────────────────────┘│
│                                    │
├────────────────────────────────────┤
│ Water Usage Comparison             │
│                                    │
│ ┌────────────────────────────────┐ │
│ │ This Season  vs.  Last Season  │ │ ← Bar chart
│ │                                │ │
│ │ ████░░░░░░    ██████████       │ │
│ │ 120L          200L             │ │
│ │                                │ │
│ │ You saved 80L (40%)            │ │
│ └────────────────────────────────┘ │
│                                    │
├────────────────────────────────────┤
│ Yield Estimation                   │
│                                    │
│ Based on stress periods:           │
│                                    │
│ Expected Yield: 2.4 tons/hectare   │
│ Last Season:    2.0 tons/hectare   │
│                                    │
│ Estimated Increase: +20%           │
│                                    │
└────────────────────────────────────┘
│ [Overview][Financial][Map][History]│
└────────────────────────────────────┘
```

---

## **5. Complete CSS Stylesheet EXAMPLE **

```css
/* ============================================
   AGRISCAN DASHBOARD - MASTER STYLESHEET
   Version: Phase 1 (Expandable to Phase 3)
   Size Target: <15KB minified
   ============================================ */

/* === CSS VARIABLES === */
:root {
  /* Status Colors */
  --status-critical: #C62828;
  --status-warning: #F9A825;
  --status-healthy: #2E7D32;
  --status-offline: #9E9E9E;
  
  /* Neutrals */
  --neutral-900: #111111;
  --neutral-700: #424242;
  --neutral-600: #666666;
  --neutral-400: #9E9E9E;
  --neutral-300: #E0E0E0;
  --neutral-200: #EEEEEE;
  --neutral-100: #F7F7F7;
  --neutral-50: #FAFAFA;
  --white: #FFFFFF;
  
  /* Chart Colors */
  --chart-primary: #2E7D32;
  --chart-secondary: #1976D2;
  --chart-fill: rgba(46, 125, 50, 0.1);
  --chart-grid: #E0E0E0;
  --chart-axis: #666666;
  
  /* Typography */
  --font-display: 48px;
  --font-h1: 24px;
  --font-h2: 18px;
  --font-h3: 16px;
  --font-body: 14px;
  --font-small: 12px;
  --font-caption: 11px;
  
  /* Spacing */
  --space-xs: 4px;
  --space-sm: 8px;
  --space-md: 16px;
  --space-lg: 24px;
  --space-xl: 32px;
  --space-2xl: 48px;
  
  /* Layout */
  --top-bar-height: 56px;
  --bottom-bar-height: 64px;
  --status-strip-height: 60px;
  
  /* Transitions */
  --transition-fast: 150ms ease-out;
  --transition-normal: 200ms ease-out;
  --transition-slow: 300ms ease-out;
}

/* === RESET & BASE === */
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

html {
  font-size: 16px;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, 
               Oxygen-Sans, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
  font-size: var(--font-body);
  line-height: 1.5;
  color: var(--neutral-900);
  background-color: var(--neutral-100);
  overflow-x: hidden;
}

/* === LAYOUT CONTAINERS === */
.app {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
  padding-top: var(--top-bar-height);
  padding-bottom: var(--bottom-bar-height);
}

.page {
  flex: 1;
  overflow-y: auto;
  -webkit-overflow-scrolling: touch;
}

.container {
  max-width: 1200px;
  margin: 0 auto;
  padding: var(--space-md);
}

/* === TOP BAR === */
.top-bar {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  height: var(--top-bar-height);
  background: var(--white);
  border-bottom: 1px solid var(--neutral-300);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 var(--space-md);
  z-index: 100;
}

.brand {
  font-size: var(--font-h2);
  font-weight: 700;
  color: var(--neutral-900);
  display: flex;
  align-items: center;
  gap: var(--space-sm);
}

.brand-icon {
  width: 24px;
  height: 24px;
  fill: var(--status-healthy);
}

.hub-status {
  display: flex;
  align-items: center;
  gap: var(--space-sm);
}

.hub-led {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--status-healthy);
  box-shadow: 0 0 8px currentColor;
  animation: pulse 2s infinite;
}

.hub-led.offline {
  background: var(--status-offline);
  animation: none;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.6; }
}

.last-update {
  font-size: var(--font-small);
  color: var(--neutral-600);
}

/* === BOTTOM TAB BAR === */
.bottom-tabs {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  height: var(--bottom-bar-height);
  background: var(--white);
  border-top: 1px solid var(--neutral-300);
  display: flex;
  z-index: 100;
}

.tab {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: var(--space-xs);
  background: none;
  border: none;
  border-top: 2px solid transparent;
  color: var(--neutral-600);
  font-size: var(--font-small);
  font-weight: 400;
  cursor: pointer;
  transition: all var(--transition-fast);
  min-height: 48px;
}

.tab:active {
  background: var(--neutral-50);
}

.tab.active {
  color: var(--neutral-900);
  font-weight: 600;
  border-top-color: var(--status-healthy);
}

.tab-icon {
  width: 24px;
  height: 24px;
  fill: currentColor;
}

/* === GLOBAL STATUS STRIP === */
.status-strip {
  width: 100%;
  height: var(--status-strip-height);
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 var(--space-md);
  margin-bottom: var(--space-lg);
}

.status-strip.critical {
  background: var(--status-critical);
}

.status-strip.warning {
  background: var(--status-warning);
}

.status-strip.healthy {
  background: var(--status-healthy);
}

.status-strip-text {
  font-size: var(--font-h1);
  font-weight: 700;
  color: var(--white);
  text-align: center;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

/* === SECTION HEADERS === */
.section-header {
  font-size: var(--font-h2);
  font-weight: 600;
  color: var(--neutral-900);
  margin-bottom: var(--space-md);
}

/* === METRIC CARDS === */
.metrics-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: var(--space-md);
  margin-bottom: var(--space-lg);
}

.metric-card {
  background: var(--white);
  border: 1px solid var(--neutral-300);
  border-radius: 8px;
  padding: var(--space-md);
  text-align: center;
  transition: box-shadow var(--transition-fast);
}

.metric-card:hover {
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
}

.metric-label {
  font-size: var(--font-small);
  color: var(--neutral-600);
  margin-bottom: var(--space-sm);
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.metric-value {
  font-size: var(--font-display);
  font-weight: 700;
  color: var(--neutral-900);
  line-height: 1;
  margin-bottom: var(--space-xs);
}

.metric-unit {
  font-size: var(--font-body);
  color: var(--neutral-600);
}

.metric-trend {
  font-size: 24px;
  margin-top: var(--space-xs);
}

.metric-trend.up {
  color: var(--status-healthy);
}

.metric-trend.down {
  color: var(--status-critical);
}

.metric-trend.stable {
  color: var(--neutral-400);
}

/* === CHART CONTAINER === */
.chart-container {
  background: var(--white);
  border: 1px solid var(--neutral-300);
  border-radius: 8px;
  padding: var(--space-md);
  margin-bottom: var(--space-lg);
}

.chart-title {
  font-size: var(--font-h3);
  font-weight: 600;
  color: var(--neutral-900);
  margin-bottom: var(--space-md);
}

.chart-canvas {
  width: 100%;
  height: 200px;
}

/* === ACTION CARDS === */
.action-card {
  background: var(--white);
  border: 1px solid var(--neutral-300);
  border-left: 4px solid var(--status-healthy);
  border-radius: 8px;
  padding: var(--space-md);
  margin-bottom: var(--space-md);
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
}

.action-card.critical {
  border-left-color: var(--status-critical);
}

.action-card.warning {
  border-left-color: var(--status-warning);
}

.action-card-header {
  display: flex;
  align-items: center;
  gap: var(--space-sm);
  margin-bottom: var(--space-sm);
}

.action-badge {
  font-size: 20px;
}

.action-title {
  font-size: var(--font-h3);
  font-weight: 600;
  color: var(--neutral-900);
  flex: 1;
}

.action-body {
  color: var(--neutral-600);
  line-height: 1.6;
  margin-bottom: var(--space-sm);
}

.action-footer {
  display: flex;
  gap: var(--space-sm);
  margin-top: var(--space-md);
}

/* === BUTTONS === */
.btn {
  padding: 12px 24px;
  border: none;
  border-radius: 8px;
  font-size: var(--font-body);
  font-weight: 600;
  cursor: pointer;
  transition: all var(--transition-fast);
  min-height: 44px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: var(--space-sm);
}

.btn-primary {
  background: var(--status-healthy);
  color: var(--white);
}

.btn-primary:hover {
  background: #256428;
}

.btn-primary:active {
  transform: scale(0.98);
}

.btn-secondary {
  background: var(--white);
  color: var(--neutral-900);
  border: 1px solid var(--neutral-300);
}

.btn-secondary:hover {
  background: var(--neutral-100);
}

.btn-ghost {
  background: transparent;
  color: var(--status-healthy);
  padding: 8px 16px;
}

.btn-ghost:hover {
  text-decoration: underline;
  background: var(--neutral-100);
}

.btn:disabled {
  background: var(--neutral-300);
  color: var(--neutral-600);
  cursor: not-allowed;
}

/* === TOGGLE BUTTONS === */
.toggle-group {
  display: inline-flex;
  border: 1px solid var(--neutral-300);
  border-radius: 8px;
  overflow: hidden;
}

.toggle-btn {
  padding: 8px 16px;
  background: var(--white);
  color: var(--neutral-600);
  border: none;
  border-right: 1px solid var(--neutral-300);
  font-size: var(--font-body);
  font-weight: 500;
  cursor: pointer;
  transition: all var(--transition-fast);
}

.toggle-btn:last-child {
  border-right: none;
}

.toggle-btn.active {
  background: var(--status-healthy);
  color: var(--white);
  font-weight: 600;
}

/* === ZONE GRID === */
.zone-grid {
  display: grid;
  grid-template-columns: repeat(8, 1fr);
  gap: var(--space-xs);
  margin-bottom: var(--space-lg);
}

.zone-cell {
  aspect-ratio: 1;
  border: 1px solid var(--neutral-300);
  border-radius: 4px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: var(--font-small);
  font-weight: 700;
  color: var(--white);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.zone-cell.critical {
  background: var(--status-critical);
}

.zone-cell.warning {
  background: var(--status-warning);
}

.zone-cell.healthy {
  background: var(--status-healthy);
}

.zone-cell.offline {
  background: var(--status-offline);
  filter: grayscale(1);
}

.zone-cell.empty {
  background: var(--neutral-100);
  cursor: default;
}

.zone-cell.active {
  border: 2px solid var(--neutral-900);
  box-shadow: 0 0 0 2px var(--white);
}

.zone-cell:hover:not(.empty) {
  transform: scale(1.05);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
}

/* === ZONE DRAWER (BOTTOM SHEET) === */
.drawer-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.4);
  z-index: 200;
  display: none;
  animation: fadeIn var(--transition-normal);
}

.drawer-overlay.active {
  display: block;
}

@keyframes fadeIn {
  from { opacity: 0; }
  to { opacity: 1; }
}

.zone-drawer {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  max-height: 60vh;
  background: var(--white);
  border-radius: 16px 16px 0 0;
  box-shadow: 0 -4px 16px rgba(0, 0, 0, 0.12);
  z-index: 201;
  transform: translateY(100%);
  transition: transform var(--transition-normal);
  overflow-y: auto;
}

.zone-drawer.active {
  transform: translateY(0);
}

.drawer-handle {
  width: 40px;
  height: 4px;
  background: var(--neutral-300);
  border-radius: 2px;
  margin: 12px auto 16px;
}

.drawer-content {
  padding: 0 var(--space-md) var(--space-lg);
}

.drawer-header {
  margin-bottom: var(--space-md);
}

.drawer-title {
  font-size: var(--font-h2);
  font-weight: 600;
  color: var(--neutral-900);
}

.drawer-subtitle {
  font-size: var(--font-small);
  color: var(--neutral-600);
  margin-top: var(--space-xs);
}

.drawer-section {
  margin-bottom: var(--space-md);
}

.drawer-label {
  font-size: var(--font-small);
  color: var(--neutral-600);
  margin-bottom: var(--space-xs);
}

.drawer-value {
  font-size: var(--font-body);
  color: var(--neutral-900);
  font-weight: 500;
}

/* === PROGRESS BAR === */
.progress-bar {
  width: 100%;
  height: 8px;
  background: var(--neutral-200);
  border-radius: 4px;
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  background: var(--status-healthy);
  transition: width var(--transition-normal);
}

.progress-fill.warning {
  background: var(--status-warning);
}

.progress-fill.critical {
  background: var(--status-critical);
}

/* === LEGEND === */
.legend {
  display: flex;
  gap: var(--space-md);
  flex-wrap: wrap;
  margin-top: var(--space-md);
}

.legend-item {
  display: flex;
  align-items: center;
  gap: var(--space-sm);
  font-size: var(--font-small);
  color: var(--neutral-600);
}

.legend-color {
  width: 16px;
  height: 16px;
  border-radius: 4px;
}

/* === TIMESTAMP === */
.timestamp {
  font-size: var(--font-small);
  color: var(--neutral-600);
  text-align: center;
  margin-top: var(--space-md);
}

/* === RESPONSIVE === */
@media (min-width: 768px) {
  .container {
    padding: var(--space-lg);
  }
  
  .metrics-grid {
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  }
  
  .zone-cell {
    font-size: var(--font-body);
  }
}

@media (max-width: 480px) {
  .top-bar {
    padding: 0 var(--space-sm);
  }
  
  .brand {
    font-size: var(--font-h3);
  }
  
  .metrics-grid {
    grid-template-columns: repeat(2, 1fr);
    gap: var(--space-sm);
  }
  
  .metric-value {
    font-size: 36px;
  }
}

/* === UTILITY CLASSES === */
.hidden {
  display: none !important;
}

.text-center {
  text-align: center;
}

.text-critical {
  color: var(--status-critical);
}

.text-warning {
  color: var(--status-warning);
}

.text-healthy {
  color: var(--status-healthy);
}

.mb-sm { margin-bottom: var(--space-sm); }
.mb-md { margin-bottom: var(--space-md); }
.mb-lg { margin-bottom: var(--space-lg); }
```

---


## **6. Language & Content Strategy**

### **6.1 Writing Style**

**Tone:** Direct, clear, reassuring
**Voice:** Active, instructional
**Reading Level:** 6th-8th grade (for low-literacy users)

**Key Principles:**
- Use **action verbs** ("Irrigate now" not "Irrigation recommended")
- Use **present tense** ("Moisture is low" not "Moisture was detected as low")
- Use **positive framing** when possible ("Water to reach optimal" not "Avoid stress")
- **No jargon** unless necessary (then define it)
- **Numbers with context** ("28% - TOO LOW" not just "28%")

### **6.2 Label & Message Library**

#### **Status Messages:**
```
Critical:
- "CRITICAL ACTION NEEDED"
- "⚠️ URGENT: Water immediately"
- "Crop stress begins in [X] hours"

Warning:
- "ATTENTION REQUIRED"
- "⚠️ Check field within 4 hours"
- "Moisture dropping gradually"

Healthy:
- "ALL ZONES HEALTHY"
- "✓ No action needed"
- "Conditions optimal"

Offline:
- "SYSTEM OFFLINE"
- "Last data: [X] minutes ago"
- "Check hub connection"
```

#### **Action Recommendations:**
```
Irrigation:
- "Irrigate Zone [X] now"
- "Apply [X] inches over [Y] hours"
- "Will bring soil to [Z]% (optimal)"

Monitoring:
- "Check again in [X] hours"
- "Monitor closely for changes"
- "Inspect field conditions"

Maintenance:
- "Replace sensor battery"
- "Sensor offline - check connection"
- "Calibration recommended"
```

#### **UI Labels:**
```
Metrics:
- "Soil Moisture" (not "Volumetric Water Content")
- "Soil Temp" (not "Soil Temperature °C")
- "Air Humidity" (not "Relative Humidity %")
- "Plant Demand" (not "Vapor Pressure Deficit")

Time Ranges:
- "Last 24 Hours" (not "24h")
- "Last 7 Days" (not "1w")
- "Last 30 Days" (not "1m")

Actions:
- "View Details" (not "More Info")
- "Ping Sensor" (not "Send Test Signal")
- "Export Data" (not "Download CSV")
```

### **6.3 Error Messages**

**Connection Errors:**
```
- "Cannot connect to hub"
- "Check if hub is powered on"
- "Try refreshing this page"
```

**Data Errors:**
```
- "Sensor not responding"
- "Data may be inaccurate"
- "Last reliable reading: [time]"
```

**User Errors:**
```
- "Please select a zone first"
- "History not available yet (need 24 hours of data)"
- "Cannot export - no data recorded"
```

---

## **7. Interaction & Animation Specifications**

### **7.1 Page Transitions**
- **Duration:** 200ms
- **Easing:** ease-out
- **Type:** Fade + slight slide (8px)
- **Loading states:** Show skeleton placeholders, not spinners

### **7.2 Button Interactions**
- **Hover:** Background darkens 10%, no transform
- **Active (tap):** Scale 0.98, background darkens 20%
- **Disabled:** 50% opacity, cursor: not-allowed

### **7.3 Drawer Behavior**
- **Open:** Slide up 200ms ease-out
- **Close:** Slide down 200ms ease-in
- **Drag handle:** User can swipe down to close
- **Overlay:** Click/tap to close drawer

### **7.4 Chart Animations**
- **Initial load:** Draw line from left to right (500ms)
- **Data update:** Smoothly shift existing data left, add new point right
- **No animation on history page load** (too much data)

### **7.5 Status Changes**
- **LED color change:** Fade between colors (300ms)
- **Status strip color change:** Fade (300ms)
- **Action card appearance:** Slide down + fade in (250ms)

---

## **8. Accessibility Requirements**

### **8.1 Touch Targets**
- **Minimum size:** 44px × 44px (Apple/Android guideline)
- **Spacing:** 8px minimum between tappable elements
- **Zone grid cells:** 40px minimum on mobile

### **8.2 Color Contrast**
- **Text on white:** Neutral-900 (#111111) = 19:1 ratio ✓
- **Text on status colors:** White on Red/Yellow/Green = 4.5:1+ ✓
- **Small text:** Use Neutral-600 minimum (7:1 ratio)

### **8.3 Font Sizing**
- **Never go below 11px** (caption text)
- **Body text default:** 14px (0.875rem)
- **Primary data:** 48px (large, readable at distance)

### **8.4 Offline Functionality**
- **All assets bundled** (no CDN dependencies)
- **Service worker** caches dashboard HTML/CSS/JS
- **Show "Offline" badge** when no internet, but continue showing cached data
- **Queue writes** to SD card if ESP32 disconnects

---

## **9. Performance Specifications**

### **9.1 File Size Targets**
```
Total Bundle (Phase 1):
- HTML: 8KB
- CSS: 15KB
- JavaScript: 20KB
- Icons (inline SVG): 5KB
---
TOTAL: ~48KB (well under 500KB limit)
```

### **9.2 Load Time Goals**
- **First paint:** <500ms
- **Interactive:** <1 second
- **Chart render:** <200ms

### **9.3 Memory Constraints**
- **ESP32 available RAM:** ~200KB for web server
- **Keep DOM minimal:** Max 50 elements on screen
- **Chart data in memory:** 144 points max (24h)

### **9.4 Update Frequency**
- **Live view:** Poll every 10 seconds
- **History:** Load once, don't auto-refresh
- **Map view:** Poll every 15 seconds (more zones = more data)

---

## **10. Progressive Enhancement by Phase**

### **Phase 0 → Phase 1:**
**What Changes:**
- Single page → 2-page navigation (add bottom tabs)
- Static thresholds → Dynamic trend calculation
- 2 metrics → 5 metrics (add DHT22 data)
- No history → 24h chart

**What Stays Same:**
- LED-first feedback
- WiFi AP connection
- SD card logging
- USB power

### **Phase 1 → Phase 2:**
**What Changes:**
- Single zone → Multi-zone support (4-8 sensors)
- Single endpoint → Multiple sensor endpoints
- 2 pages → 3 pages (add Map)
- Simple status → Zone-specific status

**What Stays Same:**
- Core metric calculations
- Chart rendering approach
- Action card logic
- Color palette

### **Phase 2 → Phase 3:**
**What Changes:**
- Sensor data only → Add financial tracking
- Offline only → Optional weather integration
- Basic recommendations → Irrigation amount calculations
- 3 pages → 5 pages (add Financial, Settings)

**What Stays Same:**
- Multi-zone grid structure
- Zone drawer pattern
- Bottom tab navigation
- Core status logic

---

## **11. Design Feel & Emotional Goals**

### **What AgriScan Should FEEL Like:**
✅ **Trustworthy** - Like a reliable tool, not a toy
✅ **Clear** - Instant understanding, zero confusion
✅ **Calm** - Not alarming unless truly critical
✅ **Professional** - Worthy of being taken seriously
✅ **Simple** - "I can use this without training"

### **What AgriScan Should NOT Feel Like:**
❌ **Overwhelming** - Too many options, too much data
❌ **Uncertain** - Vague recommendations, unclear next steps
❌ **Cheap** - Low-quality design, inconsistent spacing
❌ **Condescending** - Overly simplified, patronizing tone
❌ **Gimmicky** - Excessive animations, playful colors

### **Reference Products (Design Inspiration):**
- **Nest Thermostat:** Simple, bold numbers, clear status
- **Tesla App:** Minimal, high contrast, functional-first
- **Calm App:** Uncluttered, breathing room, soothing colors
- **Apple Health:** Data-dense but organized, clear hierarchy
- **Stripe Dashboard:** Professional, confident, trustworthy

---

## **12. Implementation Roadmap for Anthropic IDE**

### **Phase 0 (Week 1-2):**
**Build Order:**
1. Create `index.html` with basic structure
2. Add inline CSS for single-page layout
3. Write JavaScript for API polling (fetch `/api/status`)
4. Test LED color logic (mock data first)
5. Deploy to ESP32, test with real sensor

**Deliverable:** Single-page live view that updates every 10 seconds

---

### **Phase 1 (Week 3-4):**
**Build Order:**
1. Add bottom tab navigation structure
2. Create `renderOverview()` and `renderHistory()` functions
3. Implement 24h chart with pure SVG
4. Add VPD calculation logic
5. Build action card decision tree
6. Test with DHT22 sensor added

**Deliverable:** Two-page dashboard with trends and recommendations

---

### **Phase 2 (Week 5-7):**
**Build Order:**
1. Update API to support `/api/zones` endpoint (returns array)
2. Create zone grid renderer (dynamic sizing)
3. Build zone drawer component (bottom sheet)
4. Add battery level tracking to each sensor
5. Implement zone filtering in History page
6. Test with 4 sensors in different zones

**Deliverable:** Multi-zone dashboard with field map

---

### **Phase 3 (Week 8-12):**
**Build Order:**
1. Add financial tracking page structure
2. Implement irrigation amount calculator
3. Build EC sensor integration (if hardware available)
4. Add weather API integration (OpenWeather)
5. Create settings page with crop profiles
6. Build calibration helper wizard

**Deliverable:** Full platform with intelligence layer

---

## **13. File Structure for Anthropic IDE**

```
/agriscan-dashboard/
│
├── phase-0/
│   ├── index.html           (Single page MVP)
│   └── README.md            (Phase 0 notes)
│
├── phase-1/
│   ├── index.html           (Overview + History)
│   ├── css/
│   │   └── main.css         (Separated stylesheet)
│   ├── js/
│   │   ├── app.js           (Core logic)
│   │   ├── chart.js         (SVG chart renderer)
│   │   └── api.js           (Fetch functions)
│   └── README.md
│
├── phase-2/
│   ├── index.html           (Multi-zone)
│   ├── css/
│   │   └── main.css
│   ├── js/
│   │   ├── app.js
│   │   ├── chart.js
│   │   ├── api.js
│   │   ├── zones.js         (Zone grid + drawer)
│   │   └── utils.js         (Calculations)
│   └── README.md
│
└── phase-3/
    ├── index.html           (Full platform)
    ├── css/
    │   └── main.css
    ├── js/
    │   ├── app.js
    │   ├── chart.js
    │   ├── api.js
    │   ├── zones.js
    │   ├── financial.js     (ROI tracking)
    │   ├── weather.js       (API integration)
    │   └── utils.js
    └── README.md
```

---

## **14. Summary Checklist**

### **Visual Design:**
✅ Color palette defined (status + neutrals)
✅ Typography system specified (sizes, weights, line-heights)
✅ Spacing system documented (8px grid)
✅ Component dimensions provided (cards, buttons, bars)

### **Content Strategy:**
✅ Tone and voice guidelines
✅ Label library created
✅ Error message templates
✅ Writing principles established

### **Interaction Design:**
✅ Animation timing specified
✅ Touch target sizes defined
✅ Transition behaviors documented
✅ Loading states planned

### **Technical Specifications:**
✅ File size targets set
✅ Performance goals defined
✅ API endpoints outlined
✅ Memory constraints noted

### **Phase Progression:**
✅ Phase 0 scope clear
✅ Phase 1 enhancements listed
✅ Phase 2 additions defined
✅ Phase 3 advanced features outlined

---