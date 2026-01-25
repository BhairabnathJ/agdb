# **AgriScan Dashboard - Revised Specification v2.0**

---

## **1. Project Overview**

### **1.1 Design Philosophy**
**Core Principle:** *"Clarity over cleverness, function over flash"*

AgriScan serves farmers earning <$200/month in remote areas with basic phones and limited technical literacy. Every design decision prioritizes:
- **Immediate comprehension** (no learning curve)
- **Offline reliability** (works without internet)
- **Low bandwidth** (minimal file size)
- **Accessibility** (works on 320px screens to desktop, WCAG AA compliant)
- **Trust** (professional, credible, stable)
- **Agricultural identity** (feels designed for farmers, not adapted from generic dashboards)

### **1.2 Technical Constraints**
- **Platform:** ESP32 microcontroller serving static files
- **Storage:** SPIFFS/LittleFS filesystem (max 500KB total)
- **Languages:** Vanilla HTML5, CSS3, JavaScript (ES6)
- **No frameworks:** No React, Vue, Bootstrap - pure code only
- **No external dependencies:** No CDNs, no Google Fonts, no icon libraries
- **Offline-first:** All assets bundled, works without internet
- **Localization-ready:** All strings externalized from Phase 0

### **1.3 Target Devices**
Primary testing targets (budget Android devices):
- Xiaomi Redmi series
- Samsung Galaxy A series
- Tecno/Infinix devices
- Minimum viewport: 320px width
- Screen DPI: 160-320 (mdpi to xhdpi)

---

## **2. Revised Phase Structure**

### **2.1 Three-Phase Approach**

```
Phase 0: MVP
├── Single-page live view
├── Core sensor readings (moisture, temperature)
├── Physical LED status mirroring
├── Basic onboarding overlay (first-time users)
└── Offline capability

Phase 1: Field Dashboard (Combines original Phase 1 + 2)
├── Multi-zone support (up to 16 sensors)
├── Overview page with global status
├── Zone map (4x4 grid maximum on mobile)
├── History page with charts
├── Priority action cards
├── Zone detail drawer
└── Battery monitoring

Phase 2: Advanced Features (Reduced scope)
├── Extended history (30-day trends)
├── Export functionality (CSV only)
├── Sensor diagnostics and calibration
├── Alert threshold customization
└── Optional: Weather integration (when online)
```

**Note:** Financial tracking, ROI calculations, and yield estimation are explicitly OUT OF SCOPE. These features require different mental models and should be developed as a separate farm management application if needed.

### **2.2 Navigation Structure**

**Phase 0:** No navigation (single page)

**Phase 1:**
```
[Overview] [Map] [History]
```

**Phase 2:**
```
[Overview] [Map] [History] [⚙️]
```

**Navigation Type:** Fixed bottom tab bar (thumb-zone optimized)

---

## **3. Visual Design System**

### **3.1 Color Palette**

#### **Primary Colors (Functional Status)**
```css
/* Status Colors - Core System */
--status-critical: #C62828;      /* Red - Immediate action required */
--status-critical-light: #FFEBEE; /* Light red for backgrounds */

--status-warning: #F57F17;       /* Dark amber - Monitor closely (revised for contrast) */
--status-warning-light: #FFF8E1; /* Light amber for backgrounds */
--status-warning-text: #111111;  /* Dark text ON warning backgrounds */

--status-healthy: #2E7D32;       /* Green - Optimal conditions */
--status-healthy-light: #E8F5E9; /* Light green for backgrounds */

--status-offline: #757575;       /* Medium gray - Stale data / offline */
--status-offline-light: #F5F5F5; /* Light gray for backgrounds */
```

**Usage Rules:**
- **Critical (Red):** Moisture <30% OR rapid drying (>5%/hour) OR battery <10%
- **Warning (Amber):** Moisture 30-50% OR gradual drying (2-5%/hour) OR battery 10-25%
- **Healthy (Green):** Moisture >50% AND stable/increasing AND battery >25%
- **Offline (Gray):** No data for >30 minutes

**Accessibility Compliance:**
| Combination | Contrast Ratio | WCAG AA |
|-------------|----------------|---------|
| White text on Critical (#C62828) | 5.9:1 | ✓ Pass |
| White text on Healthy (#2E7D32) | 4.8:1 | ✓ Pass |
| Dark text on Warning (#F57F17) | 5.4:1 | ✓ Pass |
| White text on Offline (#757575) | 4.6:1 | ✓ Pass |

**Critical Rule:** NEVER use white text on yellow/amber backgrounds. Always use --status-warning-text (#111111).

#### **Neutral Colors (UI Structure) - Warm Palette**
```css
/* Warm Neutrals - Interface Elements */
--neutral-900: #1A1814;        /* Primary text, active icons (warm black) */
--neutral-700: #3D3833;        /* Secondary headings */
--neutral-600: #5C564E;        /* Body text, inactive states */
--neutral-400: #9C958C;        /* Disabled text, placeholders */
--neutral-300: #D4CFC7;        /* Borders, dividers */
--neutral-200: #E8E4DC;        /* Hover states */
--neutral-100: #F5F3EF;        /* Page background (warm off-white) */
--neutral-50: #FAF9F7;         /* Subtle backgrounds */
--white: #FFFDF7;              /* Warm white for cards (not pure white) */
```

#### **Chart Colors (Data Visualization)**
```css
/* Chart Lines & Fills */
--chart-moisture: #2E7D32;     /* Green - Moisture line */
--chart-temperature: #1565C0;  /* Blue - Temperature line */
--chart-fill: rgba(46, 125, 50, 0.08);  /* Healthy zone shading */
--chart-grid: #D4CFC7;         /* Gridlines (warm) */
--chart-axis: #5C564E;         /* Axis labels */
```

### **3.2 Typography**

#### **Font Family (System Native)**
```css
font-family: -apple-system, BlinkMacSystemFont,
             "Segoe UI", Roboto, Oxygen-Sans, Ubuntu,
             Cantarell, "Helvetica Neue", sans-serif;
```

#### **Revised Type Scale (Increased Minimums)**
```css
/* Display - Hero Numbers */
--font-display: 48px;          /* 3rem - Large sensor readings */
--font-display-weight: 700;

/* H1 - Page Titles / Status */
--font-h1: 24px;              /* 1.5rem - Global status text */
--font-h1-weight: 700;

/* H2 - Section Headers */
--font-h2: 20px;              /* 1.25rem - Card titles (increased) */
--font-h2-weight: 600;

/* H3 - Sub-headers */
--font-h3: 18px;              /* 1.125rem - Zone labels, metric names (increased) */
--font-h3-weight: 600;

/* Body - Primary Text */
--font-body: 16px;            /* 1rem - Default text (INCREASED from 14px) */
--font-body-weight: 500;      /* Medium weight for legibility */

/* Small - Metadata */
--font-small: 14px;           /* 0.875rem - Timestamps, captions (INCREASED from 12px) */
--font-small-weight: 500;

/* Caption - Minimum Readable */
--font-caption: 14px;         /* 0.875rem - Chart axes (INCREASED from 11px) */
--font-caption-weight: 400;
```

**Critical Rule:** No text smaller than 14px anywhere in the interface.

#### **Line Heights**
```css
--line-height-tight: 1.2;     /* Headlines, data values */
--line-height-normal: 1.5;    /* Body text */
--line-height-relaxed: 1.6;   /* Action card descriptions */
```

#### **Font Usage Matrix (Revised)**

| Element | Size | Weight | Line Height | Color |
|---------|------|--------|-------------|-------|
| Global Status Text | 24px | 700 | 1.2 | White (on green/red) or Dark (on amber) |
| Sensor Reading Value | 48px | 700 | 1.0 | Neutral-900 |
| Metric Label | 14px | 500 | 1.2 | Neutral-600 |
| Card Title | 20px | 600 | 1.2 | Neutral-900 |
| Action Card Message | 18px | 600 | 1.5 | Neutral-900 |
| Body Text | 16px | 500 | 1.5 | Neutral-600 |
| Timestamp | 14px | 500 | 1.2 | Neutral-600 |
| Button Text | 16px | 600 | 1.2 | White/Neutral-900 |
| Tab Label | 14px | 600 | 1.2 | Neutral-600/900 |
| Chart Axis Labels | 14px | 400 | 1.2 | Neutral-600 |

### **3.3 Spacing System (8px Grid)**

```css
/* Spacing Scale */
--space-xs: 4px;      /* 0.25rem - Tight internal padding */
--space-sm: 8px;      /* 0.5rem - Icon margins, small gaps */
--space-md: 16px;     /* 1rem - Default padding, standard gaps */
--space-lg: 24px;     /* 1.5rem - Section spacing */
--space-xl: 32px;     /* 2rem - Page margins on tablet+ */
--space-2xl: 48px;    /* 3rem - Major section breaks */
```

**Usage Guidelines:**
- **Card padding:** 16px (md)
- **Card border-radius:** 12px (increased for organic feel)
- **Page margins:** 16px mobile, 24px tablet+
- **Section gaps:** 24px between major sections
- **Button padding:** 14px vertical, 24px horizontal
- **Touch target minimum:** 48px height (increased from 44px)

### **3.4 Visual Character**

#### **Background Texture**
Apply subtle topographic/contour pattern at 3-5% opacity on page background to create agricultural sense of place:
```css
.page-background {
  background-color: var(--neutral-100);
  background-image: url('data:image/svg+xml,...'); /* Inline SVG contour pattern */
  background-size: 200px 200px;
  background-repeat: repeat;
}
```

#### **Border Radius Scale**
```css
--radius-sm: 8px;     /* Buttons, inputs */
--radius-md: 12px;    /* Cards, containers (increased from 8px) */
--radius-lg: 16px;    /* Drawers, modals */
--radius-full: 9999px; /* Pills, badges */
```

#### **Shadow Scale**
```css
--shadow-sm: 0 1px 3px rgba(26, 24, 20, 0.08);
--shadow-md: 0 4px 12px rgba(26, 24, 20, 0.1);
--shadow-lg: 0 8px 24px rgba(26, 24, 20, 0.12);
```

---

## **4. Component Specifications**

### **4.1 Top Bar (Fixed Header)**
```
Height: 56px
Background: var(--white)
Border-bottom: 1px solid var(--neutral-300)
Z-index: 100

Layout:
┌────────────────────────────────────┐
│ ■ AGRISCAN    [●] 2 min ago       │
│ (Logo)        (LED) (Time)         │
└────────────────────────────────────┘

Components:
- Logo/Brand (Left): 20px height, Neutral-900
- Hub Status LED (Right): 10px circle with glow animation
- Last Update Text (Right): 14px, Neutral-600
```

#### **LED Indicator Styles**
```css
.hub-led {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: var(--status-healthy);
  box-shadow: 0 0 8px var(--status-healthy);
  animation: led-pulse 2s ease-in-out infinite;
}

@keyframes led-pulse {
  0%, 100% { opacity: 1; box-shadow: 0 0 8px currentColor; }
  50% { opacity: 0.7; box-shadow: 0 0 4px currentColor; }
}

.hub-led.critical {
  background: var(--status-critical);
  animation: led-pulse-fast 1s ease-in-out infinite;
}

.hub-led.offline {
  background: var(--status-offline);
  animation: none;
  box-shadow: none;
}
```

### **4.2 Bottom Tab Bar (Fixed Navigation)**
```
Height: 72px (increased for larger touch targets)
Background: var(--white)
Border-top: 1px solid var(--neutral-300)
Z-index: 100
Safe area padding: env(safe-area-inset-bottom) for notched phones

Layout:
┌────────────────────────────────────┐
│  [Overview]    [Map]    [History]  │
│    (Icon)     (Icon)     (Icon)    │
│    Label      Label      Label     │
└────────────────────────────────────┘

Tab Touch Target: 48px minimum height, full flex width
```

#### **Tab States**
```css
/* Inactive Tab */
.tab {
  color: var(--neutral-600);
  font-weight: 500;
  border-top: 3px solid transparent;
}

/* Active Tab */
.tab.active {
  color: var(--neutral-900);
  font-weight: 600;
  border-top-color: var(--status-healthy);
}

/* Active Tab Icon */
.tab.active .tab-icon {
  fill: var(--status-healthy);
}
```

#### **Tab Icons (Inline SVG)**
```html
<!-- Overview Icon (Home/Dashboard) -->
<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/>
</svg>

<!-- Map Icon (Grid) -->
<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor">
  <path d="M3 3h8v8H3zm10 0h8v8h-8zM3 13h8v8H3zm10 0h8v8h-8z"/>
</svg>

<!-- History Icon (Chart) -->
<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor">
  <path d="M3 13h2v8H3zm4-4h2v12H7zm4-4h2v16h-2zm4 2h2v14h-2zm4-6h2v20h-2z"/>
</svg>

<!-- Settings Icon (Gear) - Phase 2 -->
<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor">
  <path d="M19.14 12.94c.04-.31.06-.63.06-.94 0-.31-.02-.63-.06-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/>
</svg>
```

### **4.3 Global Status Strip**
```
Height: 64px
Full width
Padding: 0 16px
Margin-bottom: 24px
Border-radius: 0 (full bleed)

Layout:
┌────────────────────────────────────┐
│                                    │
│     ALL ZONES HEALTHY              │ ← Green bg, white text
│                                    │
└────────────────────────────────────┘

Or (Warning state - note dark text):
┌────────────────────────────────────┐
│                                    │
│  ⚠ ATTENTION REQUIRED              │ ← Amber bg, DARK text
│                                    │
└────────────────────────────────────┘
```

**ARIA Requirements:**
```html
<div class="status-strip healthy" role="status" aria-live="polite" aria-atomic="true">
  <span class="status-strip-text">All Zones Healthy</span>
</div>
```

### **4.4 Metric Cards**

#### **Layout**
```
Grid: 2 columns on mobile, 3+ on tablet
Gap: 16px
Card padding: 16px
Border-radius: 12px
Background: var(--white)
Border: 1px solid var(--neutral-300)
```

#### **Visual Structure**
```
┌──────────────────┐
│ SOIL MOISTURE    │ ← Label: 14px, uppercase, Neutral-600
│                  │
│      45%         │ ← Value: 48px bold, Neutral-900
│                  │
│      ↓↓          │ ← Trend: 24px, Status color
│   Dropping       │ ← Trend label: 14px, Status color
└──────────────────┘
```

#### **Status Background Tinting**
Cards receive subtle background tint based on current status:
```css
.metric-card {
  background: var(--white);
  transition: background-color 300ms ease;
}

.metric-card.critical {
  background: var(--status-critical-light); /* #FFEBEE */
}

.metric-card.warning {
  background: var(--status-warning-light); /* #FFF8E1 */
}

.metric-card.healthy {
  background: var(--white); /* No tint when healthy */
}
```

#### **Trend Indicators**
```
↑↑  Rapid increase   → status-healthy + "Rising fast"
↑   Gradual increase → status-healthy + "Rising"
→   Stable           → neutral-400 + "Stable"
↓   Gradual decrease → status-warning + "Dropping"
↓↓  Rapid decrease   → status-critical + "Dropping fast"
```

### **4.5 Action Cards (Priority Alerts)**

#### **Layout**
```
Width: Full width - 32px margins
Padding: 16px
Border-radius: 12px
Background: var(--white)
Border-left: 4px solid [status-color]
Box-shadow: var(--shadow-md)
```

#### **Visual Structure**
```
┌────────────────────────────────────┐
│ ● IRRIGATE ZONE B3                 │ ← CSS circle + Title: 18px bold
│                                    │
│ Critical moisture level.           │ ← Body: 16px, Neutral-600
│ Apply 0.5" water over 2 hours.     │
│                                    │
│ [View Zone]                        │ ← Ghost button
└────────────────────────────────────┘
```

#### **Status Badges (CSS Circles, NOT Emoji)**
```css
.action-badge {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  flex-shrink: 0;
}

.action-badge.critical {
  background: var(--status-critical);
}

.action-badge.warning {
  background: var(--status-warning);
}

.action-badge.healthy {
  background: var(--status-healthy);
}
```

**Critical Rule:** Never use emoji (🔴🟡🟢) for status indicators. They render inconsistently across devices.

### **4.6 Zone Grid (Phase 1)**

#### **Mobile Constraints**
```
Maximum grid: 4x4 (16 zones) on screens <600px
Cell size: Flexible, minimum 60px × 60px
Gap: 8px
Touch target: 60px minimum (exceeds 48px requirement)
```

#### **Layout**
```
┌──────────────────────────────────┐
│  Field Zones (8 Active)          │
│                                  │
│  ┌────┬────┬────┬────┐          │
│  │ A1 │ A2 │ A3 │ A4 │          │
│  ├────┼────┼────┼────┤          │
│  │ B1 │ B2 │ B3 │ B4 │          │
│  ├────┼────┼────┼────┤          │
│  │ C1 │ C2 │ C3 │ C4 │          │
│  ├────┼────┼────┼────┤          │
│  │ D1 │ D2 │ D3 │ D4 │          │
│  └────┴────┴────┴────┘          │
│                                  │
│  ● Optimal  ● Warning  ● Critical│
└──────────────────────────────────┘
```

#### **Cell States**
```css
.zone-cell {
  aspect-ratio: 1;
  min-width: 60px;
  min-height: 60px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 16px;
  font-weight: 700;
  color: var(--white);
  cursor: pointer;
  transition: transform 150ms ease, box-shadow 150ms ease;
}

.zone-cell:active {
  transform: scale(0.95);
}

.zone-cell.critical { background: var(--status-critical); }
.zone-cell.warning {
  background: var(--status-warning);
  color: var(--neutral-900); /* Dark text on amber */
}
.zone-cell.healthy { background: var(--status-healthy); }
.zone-cell.offline {
  background: var(--status-offline);
  opacity: 0.6;
}
.zone-cell.empty {
  background: var(--neutral-200);
  cursor: default;
  color: var(--neutral-400);
}

/* Selected state */
.zone-cell.selected {
  box-shadow: 0 0 0 3px var(--neutral-900), 0 0 0 5px var(--white);
}

/* Low battery indicator */
.zone-cell.low-battery::after {
  content: '';
  position: absolute;
  top: 4px;
  right: 4px;
  width: 8px;
  height: 8px;
  background: var(--status-warning);
  border-radius: 50%;
  border: 1px solid var(--white);
}
```

#### **ARIA Requirements**
```html
<div class="zone-grid" role="grid" aria-label="Field zone status map">
  <div class="zone-row" role="row">
    <button class="zone-cell healthy" role="gridcell"
            aria-label="Zone A1, moisture 65%, healthy"
            aria-describedby="zone-a1-details">
      A1
    </button>
    <!-- ... -->
  </div>
</div>
```

### **4.7 Zone Drawer (Bottom Sheet)**

#### **Layout**
```
Max height: 70vh
Background: var(--white)
Border-radius: 16px 16px 0 0
Box-shadow: var(--shadow-lg)
Z-index: 201
```

#### **Visual Structure**
```
┌────────────────────────────────────┐
│         ════════                   │ ← Drag handle (decorative)
│         [×]                        │ ← EXPLICIT close button (required)
├────────────────────────────────────┤
│ Zone B3                            │ ← Title: 20px bold
│ Sensor ID: 0x4F2A                  │ ← Subtitle: 14px, Neutral-600
│                                    │
│ Battery                            │
│ [████████░░░░] 67%                 │ ← Progress bar + percentage
│                                    │
│ ┌────────────┬────────────┐       │
│ │ Moisture   │ Soil Temp  │       │ ← 2-column mini metrics
│ │   28%      │   24°C     │       │
│ │  CRITICAL  │  NORMAL    │       │
│ └────────────┴────────────┘       │
│                                    │
│ Last reading: 1 min ago            │
│                                    │
│ [View History]  [Ping Sensor]      │ ← Action buttons
└────────────────────────────────────┘
```

#### **Close Button (Required)**
```html
<button class="drawer-close" aria-label="Close zone details">
  <svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor">
    <path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/>
  </svg>
</button>
```

**Critical Rule:** Always provide an explicit close button. Drag-to-close is a supplementary gesture, not the primary close mechanism.

### **4.8 Chart Container**

#### **Layout**
```
Height: 220px (chart area) + 40px (controls) = 260px total
Padding: 16px
Background: var(--white)
Border: 1px solid var(--neutral-300)
Border-radius: 12px
```

#### **Chart Specifications**
```
SVG viewBox: "0 0 400 180"
Line stroke: 2.5px
Fill opacity: 8% (healthy zone band)
Grid lines: 1px, var(--chart-grid)
Axis labels: 14px, var(--chart-axis)
Data points: 144 max (10-minute intervals over 24h)
```

#### **Touch-to-Inspect Tooltip**
```
┌──────────────────────────────────────────┐
│                           ┌─────────┐    │
│     ╱╲      ╱╲           │ 45%     │    │ ← Tooltip appears on touch
│    ╱  ╲    ╱  ╲     ●────│ 2:30 PM │    │
│   ╱    ╲──╱    ╲────     └─────────┘    │
│  ╱                  ╲                    │
│ ════════════════════════════════════     │ ← Healthy zone band (50-70%)
│ 12am    6am    12pm    6pm    12am       │
└──────────────────────────────────────────┘
```

**Interaction:**
- Touch anywhere on chart → show nearest data point tooltip
- Tooltip shows value + timestamp
- Tooltip follows touch/drag horizontally
- Release touch → tooltip fades after 2 seconds

#### **ARIA Requirements**
```html
<figure class="chart-container" role="img"
        aria-label="Soil moisture over last 24 hours, ranging from 42% to 68%, currently at 45% and dropping">
  <figcaption class="sr-only">
    Line chart showing soil moisture percentage over the last 24 hours.
    Lowest point: 42% at 6:00 AM. Highest point: 68% at 2:00 PM.
    Current reading: 45%, trend: dropping.
  </figcaption>
  <svg><!-- Chart SVG --></svg>
</figure>
```

### **4.9 Buttons**

#### **Primary Button**
```css
.btn-primary {
  padding: 14px 24px;
  min-height: 48px;
  background: var(--status-healthy);
  color: var(--white);
  border: none;
  border-radius: 8px;
  font-size: 16px;
  font-weight: 600;
  cursor: pointer;
  transition: background 150ms ease, transform 100ms ease;
}

.btn-primary:hover {
  background: #256428; /* 10% darker */
}

.btn-primary:active {
  transform: scale(0.98);
  background: #1B4D1E; /* 20% darker */
}

.btn-primary:disabled {
  background: var(--neutral-300);
  color: var(--neutral-600);
  cursor: not-allowed;
  transform: none;
}
```

#### **Secondary Button**
```css
.btn-secondary {
  padding: 14px 24px;
  min-height: 48px;
  background: var(--white);
  color: var(--neutral-900);
  border: 1px solid var(--neutral-300);
  border-radius: 8px;
  font-size: 16px;
  font-weight: 600;
}

.btn-secondary:hover {
  background: var(--neutral-100);
}

.btn-secondary:active {
  background: var(--neutral-200);
}
```

#### **Ghost Button**
```css
.btn-ghost {
  padding: 12px 16px;
  min-height: 48px;
  background: transparent;
  color: var(--status-healthy);
  border: none;
  font-size: 16px;
  font-weight: 600;
}

.btn-ghost:hover {
  text-decoration: underline;
  background: var(--neutral-50);
}
```

### **4.10 Toggle Button Group**
```css
.toggle-group {
  display: inline-flex;
  border: 1px solid var(--neutral-300);
  border-radius: 8px;
  overflow: hidden;
}

.toggle-btn {
  padding: 12px 20px;
  min-height: 48px;
  background: var(--white);
  color: var(--neutral-600);
  border: none;
  border-right: 1px solid var(--neutral-300);
  font-size: 16px;
  font-weight: 500;
  cursor: pointer;
}

.toggle-btn:last-child {
  border-right: none;
}

.toggle-btn.active {
  background: var(--status-healthy);
  color: var(--white);
  font-weight: 600;
}
```

### **4.11 Progress Bar (Battery)**
```css
.progress-bar {
  width: 100%;
  height: 10px;
  background: var(--neutral-200);
  border-radius: 5px;
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  background: var(--status-healthy);
  border-radius: 5px;
  transition: width 300ms ease;
}

.progress-fill.warning {
  background: var(--status-warning);
}

.progress-fill.critical {
  background: var(--status-critical);
}
```

---

## **5. State Specifications**

### **5.1 Loading States**

#### **Skeleton Placeholders**
Show skeleton shapes instead of spinners. Skeletons match the shape of content being loaded.

```css
.skeleton {
  background: linear-gradient(
    90deg,
    var(--neutral-200) 25%,
    var(--neutral-100) 50%,
    var(--neutral-200) 75%
  );
  background-size: 200% 100%;
  animation: skeleton-shimmer 1.5s ease-in-out infinite;
  border-radius: 8px;
}

@keyframes skeleton-shimmer {
  0% { background-position: 200% 0; }
  100% { background-position: -200% 0; }
}
```

#### **Skeleton Examples**
```
Metric Card Loading:
┌──────────────────┐
│ ████████         │ ← Label skeleton
│                  │
│   ████████       │ ← Value skeleton
│                  │
│     ████         │ ← Trend skeleton
└──────────────────┘

Chart Loading:
┌────────────────────────────────────┐
│ ████████                           │
│                                    │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │ ← Shimmer animation
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│                                    │
└────────────────────────────────────┘
```

### **5.2 Error States**

#### **Component Error (Data Fetch Failed)**
```
┌──────────────────┐
│ SOIL MOISTURE    │
│                  │
│      --          │ ← Double dash indicates no data
│                  │
│  ⚠ Cannot load   │ ← Warning icon (CSS) + message
│  [Retry]         │ ← Retry button
└──────────────────┘
```

#### **Connection Error (Full Page)**
```
┌────────────────────────────────────┐
│                                    │
│         ┌─────────────┐           │
│         │     ╳       │           │ ← Disconnected icon
│         └─────────────┘           │
│                                    │
│     Cannot connect to hub          │
│                                    │
│   1. Check if hub is powered on    │
│   2. Verify WiFi connection        │
│   3. Try refreshing this page      │
│                                    │
│         [Retry Connection]         │
│                                    │
│   Last successful: 5 min ago       │
│                                    │
└────────────────────────────────────┘
```

#### **Stale Data Warning**
When data is older than 30 minutes but connection exists:
```
┌──────────────────┐
│ SOIL MOISTURE    │
│    ⚠ Stale       │ ← Warning badge
│      45%         │ ← Show last known value
│                  │
│  Updated 45m ago │ ← Emphasized timestamp
└──────────────────┘
```

### **5.3 Empty States**

#### **No Sensors Configured**
```
┌────────────────────────────────────┐
│                                    │
│         ┌─────────────┐           │
│         │   📡        │           │ ← Sensor icon
│         └─────────────┘           │
│                                    │
│      No sensors detected           │
│                                    │
│   Make sure sensors are powered    │
│   on and within range of the hub.  │
│                                    │
│   [Scan for Sensors]               │
│                                    │
└────────────────────────────────────┘
```

#### **No History Data**
```
┌────────────────────────────────────┐
│                                    │
│   History will appear here once    │
│   24 hours of data is collected.   │
│                                    │
│   Started collecting: 6 hours ago  │
│   Ready in: ~18 hours              │
│                                    │
└────────────────────────────────────┘
```

---

## **6. Responsive Layouts**

### **6.1 Breakpoints**
```css
/* Mobile first - default styles */
/* Targets: 320px - 599px */

@media (min-width: 600px) {
  /* Tablet */
  /* Targets: 600px - 1023px */
}

@media (min-width: 1024px) {
  /* Desktop */
  /* Targets: 1024px+ */
}
```

### **6.2 Layout Adaptations**

#### **Mobile (320px - 599px)**
```
- 2-column metric grid
- 4x4 zone grid maximum
- Full-width action cards
- Stacked chart controls
- Bottom sheet drawer (70vh max)
```

#### **Tablet (600px - 1023px)**
```
- 3-column metric grid
- 4x4 zone grid with larger cells (80px)
- Side-by-side chart controls
- Increased page margins (24px)
```

#### **Desktop (1024px+)**
```
- 4-5 column metric grid
- Full zone grid if needed
- Max content width: 1200px, centered
- Drawer becomes side panel (right-aligned, 400px width)
```

### **6.3 Landscape Orientation**

When viewport width > height (landscape mode):

```css
@media (orientation: landscape) and (max-height: 500px) {
  /* Compact chrome */
  .top-bar { height: 48px; }
  .bottom-tabs { height: 56px; }

  /* Side-by-side layout */
  .overview-content {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: var(--space-lg);
  }

  /* Reduce metric card height */
  .metric-value { font-size: 36px; }
}
```

---

## **7. Onboarding Flow (First-Time Users)**

### **7.1 Welcome Overlay**
Shown on first visit (tracked via localStorage):

```
┌────────────────────────────────────┐
│                                    │
│        Welcome to AgriScan         │
│                                    │
│   Your soil monitoring dashboard   │
│                                    │
│  ┌──────────────────────────────┐ │
│  │ ● ○ ○                        │ │ ← Step indicator
│  │                              │ │
│  │    ┌────┐ ┌────┐            │ │
│  │    │ 45%│ │22°C│            │ │
│  │    └────┘ └────┘            │ │
│  │                              │ │
│  │  These cards show your       │ │
│  │  current soil readings.      │ │
│  │                              │ │
│  └──────────────────────────────┘ │
│                                    │
│           [Next →]                 │
│                                    │
│      Skip introduction             │ ← Ghost link
│                                    │
└────────────────────────────────────┘
```

### **7.2 Onboarding Steps (3 Total)**

1. **Reading the cards** - What the metric values mean
2. **Understanding colors** - Green/Yellow/Red status system
3. **Taking action** - What to do when you see warnings

### **7.3 Implementation**
- Total: 3 steps maximum (low cognitive load)
- Each step: single focused concept
- Skip option always visible
- Completion stored in localStorage
- Can be re-triggered from Settings (Phase 2)

---

## **8. Accessibility Specifications**

### **8.1 ARIA Landmarks**
```html
<body>
  <header role="banner" class="top-bar">...</header>

  <main role="main" id="main-content">
    <section aria-labelledby="status-heading">
      <h1 id="status-heading" class="sr-only">Current Status</h1>
      <div class="status-strip" role="status" aria-live="polite">...</div>
    </section>

    <section aria-labelledby="metrics-heading">
      <h2 id="metrics-heading">Live Metrics</h2>
      <div class="metrics-grid">...</div>
    </section>
  </main>

  <nav role="navigation" aria-label="Main navigation" class="bottom-tabs">
    ...
  </nav>
</body>
```

### **8.2 Live Regions**
```html
<!-- Status changes announce automatically -->
<div class="status-strip" role="status" aria-live="polite" aria-atomic="true">
  <span class="status-text">All Zones Healthy</span>
</div>

<!-- Critical alerts announce immediately -->
<div class="alert-container" role="alert" aria-live="assertive">
  <!-- Populated dynamically when critical status occurs -->
</div>
```

### **8.3 Focus Management**
```css
/* Visible focus indicators */
:focus {
  outline: 3px solid var(--status-healthy);
  outline-offset: 2px;
}

:focus:not(:focus-visible) {
  outline: none;
}

:focus-visible {
  outline: 3px solid var(--status-healthy);
  outline-offset: 2px;
}
```

### **8.4 Screen Reader Only Content**
```css
.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border: 0;
}
```

### **8.5 Touch Target Requirements**
- Minimum touch target: 48px × 48px
- Minimum spacing between targets: 8px
- Zone grid cells: 60px × 60px minimum

---

## **9. Content & Language**

### **9.1 Terminology Glossary (Strict)**

Use these terms exactly as specified:

| Preferred Term | Never Use |
|----------------|-----------|
| Soil Moisture | Moisture, Volumetric Water Content, VWC |
| Soil Temperature | Soil Temp, Ground Temperature |
| Air Temperature | Air Temp, Ambient Temperature |
| Air Humidity | Humidity, Relative Humidity, RH |
| Zone | Sensor, Node, Point |
| Hub | Gateway, Base Station, Controller |
| Reading | Value, Measurement, Data Point |

### **9.2 Status Messages**

#### **Critical**
```
Primary: "CRITICAL - ACTION NEEDED"
Detail: "Zone [X] requires immediate irrigation"
Action: "Irrigate Zone [X] now"
```

#### **Warning**
```
Primary: "ATTENTION REQUIRED"
Detail: "Zone [X] moisture dropping"
Action: "Check Zone [X] within 4 hours"
```

#### **Healthy**
```
Primary: "ALL ZONES HEALTHY"
Detail: "No action needed"
Action: "Next check recommended: [time]"
```

#### **Offline**
```
Primary: "CONNECTION LOST"
Detail: "Cannot reach hub"
Action: "Check hub power and try again"
```

### **9.3 Time Formatting**
```
Relative (preferred):
- "Just now" (< 1 minute)
- "2 min ago" (< 60 minutes)
- "1 hour ago" (< 24 hours)
- "Yesterday, 3:00 PM" (< 7 days)
- "Jan 15, 2:30 PM" (> 7 days)

Durations:
- "in 3 hours" (future)
- "for 2 hours" (duration)
```

### **9.4 Number Formatting**
```
Percentages: "45%" (no decimal for moisture)
Temperature: "22°C" or "72°F" (based on settings)
Battery: "67%" (no decimal)
Large numbers: "1,234" (comma separators)
```

### **9.5 Localization Architecture**
All user-facing strings externalized to JSON:
```javascript
// strings/en.json
{
  "status": {
    "healthy": "All Zones Healthy",
    "warning": "Attention Required",
    "critical": "Critical - Action Needed",
    "offline": "Connection Lost"
  },
  "metrics": {
    "soilMoisture": "Soil Moisture",
    "soilTemperature": "Soil Temperature",
    "airHumidity": "Air Humidity"
  },
  "actions": {
    "irrigate": "Irrigate Zone {{zone}} now",
    "check": "Check Zone {{zone}} within {{hours}} hours"
  }
}
```

---

## **10. Animation & Transitions**

### **10.1 Timing Constants**
```css
--duration-instant: 100ms;   /* Button press feedback */
--duration-fast: 150ms;      /* Hover states */
--duration-normal: 200ms;    /* Page transitions */
--duration-slow: 300ms;      /* Drawer open/close */
--duration-emphasis: 500ms;  /* Chart draw, status change */

--easing-default: ease-out;
--easing-bounce: cubic-bezier(0.34, 1.56, 0.64, 1);
```

### **10.2 Specific Animations**

#### **LED Pulse**
```css
@keyframes led-pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.6; }
}
/* Duration: 2s, infinite */
```

#### **Status Strip Color Change**
```css
.status-strip {
  transition: background-color 300ms ease-out;
}
```

#### **Drawer Slide**
```css
.zone-drawer {
  transform: translateY(100%);
  transition: transform 300ms cubic-bezier(0.32, 0.72, 0, 1);
}

.zone-drawer.open {
  transform: translateY(0);
}
```

#### **Chart Line Draw (Initial Load)**
```css
.chart-line {
  stroke-dasharray: 1000;
  stroke-dashoffset: 1000;
  animation: draw-line 800ms ease-out forwards;
}

@keyframes draw-line {
  to { stroke-dashoffset: 0; }
}
```

#### **Skeleton Shimmer**
```css
@keyframes skeleton-shimmer {
  0% { background-position: 200% 0; }
  100% { background-position: -200% 0; }
}
/* Duration: 1.5s, infinite */
```

### **10.3 Reduced Motion**
```css
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
  }
}
```

---

## **11. Data & Performance**

### **11.1 File Size Budget**

| Asset | Phase 0 | Phase 1 | Phase 2 |
|-------|---------|---------|---------|
| HTML | 6KB | 10KB | 12KB |
| CSS | 10KB | 16KB | 18KB |
| JavaScript | 12KB | 30KB | 40KB |
| Icons (inline) | 2KB | 4KB | 5KB |
| Strings (JSON) | 1KB | 2KB | 3KB |
| **Total** | **31KB** | **62KB** | **78KB** |

All sizes are minified + gzipped targets. Raw sizes approximately 2.5x larger.

### **11.2 API Endpoints**

#### **Phase 0**
```
GET /api/status
Response: {
  "moisture": 45,
  "temperature": 22,
  "status": "healthy",
  "ledColor": "green",
  "timestamp": 1706112000
}
```

#### **Phase 1**
```
GET /api/zones
Response: {
  "zones": [
    {
      "id": "A1",
      "moisture": 45,
      "temperature": 22,
      "humidity": 65,
      "battery": 87,
      "status": "healthy",
      "lastReading": 1706112000
    },
    ...
  ],
  "globalStatus": "healthy",
  "timestamp": 1706112000
}

GET /api/history?zone=A1&range=24h
Response: {
  "zone": "A1",
  "metric": "moisture",
  "points": [
    { "value": 45, "timestamp": 1706112000 },
    ...
  ]
}
```

### **11.3 Polling Intervals**
```
Overview page: Every 10 seconds
Map page: Every 15 seconds
History page: No polling (load once)
Background tab: Pause polling, resume on focus
```

### **11.4 Cache Strategy**
```javascript
// Service Worker Cache
CACHE_VERSION = 'agriscan-v1';

// Cache first (static assets)
- /index.html
- /css/main.css
- /js/app.js
- /strings/*.json

// Network first (API data)
- /api/*

// Stale data threshold
STALE_THRESHOLD = 30 * 60 * 1000; // 30 minutes
```

### **11.5 Data Retention**
```
On-device (ESP32 SD card):
- Raw readings: 30 days rolling
- Hourly averages: 1 year
- Daily summaries: Indefinite

Browser (localStorage):
- User preferences: Indefinite
- Onboarding status: Indefinite
- Last viewed zone: Session only
```

### **11.6 Memory Management**
```
Maximum DOM elements: 100
Chart data points in memory: 144 (24h at 10-min intervals)
Zone data objects: 16 maximum

Cleanup triggers:
- Page hidden > 5 minutes: Clear chart data
- Session > 30 minutes: Reload page data
- Low memory warning: Reduce to essential data only
```

---

## **12. Page Layouts**

### **12.1 Phase 0: Live View**

```
┌────────────────────────────────────┐
│ ■ AGRISCAN          [●] 30 sec ago │ ← Top bar
├────────────────────────────────────┤
│                                    │
│         [●] System Online          │ ← LED mirror (large)
│                                    │
├────────────────────────────────────┤
│                                    │
│  ┌──────────┐  ┌──────────┐       │
│  │ SOIL     │  │ SOIL     │       │
│  │ MOISTURE │  │ TEMP     │       │ ← Metric cards
│  │          │  │          │       │   (2-column)
│  │   45%    │  │   22°C   │       │
│  │    ↓     │  │    →     │       │
│  │ Dropping │  │  Stable  │       │
│  └──────────┘  └──────────┘       │
│                                    │
├────────────────────────────────────┤
│                                    │
│    Conditions: Optimal             │ ← Status summary
│    No action needed                │
│                                    │
├────────────────────────────────────┤
│                                    │
│    Last updated: 30 seconds ago    │ ← Timestamp
│                                    │
└────────────────────────────────────┘
```

### **12.2 Phase 1: Overview Page**

```
┌────────────────────────────────────┐
│ ■ AGRISCAN          [●] 2 min ago  │
├────────────────────────────────────┤
│                                    │
│       ALL ZONES HEALTHY            │ ← Status strip
│                                    │
├────────────────────────────────────┤
│ Live Metrics                       │
│                                    │
│ ┌────────┐ ┌────────┐ ┌────────┐  │
│ │  45%   │ │  22°C  │ │  65%   │  │ ← 3-column grid
│ │Moisture│ │SoilTemp│ │Humidity│  │   (wraps to 2 on mobile)
│ │   ↓    │ │   →    │ │   ↑    │  │
│ └────────┘ └────────┘ └────────┘  │
│                                    │
├────────────────────────────────────┤
│ 24h Trend                          │
│ ┌────────────────────────────────┐ │
│ │70%                             │ │
│ │    ╱╲      ╱╲       ╱╲        │ │ ← Chart
│ │   ╱  ╲    ╱  ╲     ╱  ╲       │ │
│ │  ╱    ╲──╱    ╲───╱    ╲──    │ │
│ │30%                             │ │
│ │ 12am   6am   12pm   6pm  Now  │ │
│ └────────────────────────────────┘ │
│                                    │
├────────────────────────────────────┤
│ Priority Actions                   │
│ ┌────────────────────────────────┐ │
│ │● All zones optimal             │ │ ← Action card
│ │  No action needed at this time │ │
│ │  Next check: 4 hours           │ │
│ └────────────────────────────────┘ │
│                                    │
├────────────────────────────────────┤
│  [Overview]    [Map]    [History]  │ ← Bottom tabs
└────────────────────────────────────┘
```

### **12.3 Phase 1: Map Page**

```
┌────────────────────────────────────┐
│ ■ AGRISCAN          [●] 2 min ago  │
├────────────────────────────────────┤
│                                    │
│ Field Zones (8 Active)             │
│                                    │
│ ┌────┬────┬────┬────┐             │
│ │ A1 │ A2 │ A3 │ A4 │             │
│ │ 🟢 │ 🟢 │ 🟡 │ 🟢 │             │ ← 4x4 grid
│ ├────┼────┼────┼────┤             │   (CSS circles, not emoji)
│ │ B1 │ B2 │ B3 │ B4 │             │
│ │ 🟢 │ 🟢 │ 🔴 │ 🟢 │             │
│ ├────┼────┼────┼────┤             │
│ │ C1 │ C2 │    │    │             │
│ │ 🟢 │ 🟡 │    │    │             │
│ ├────┼────┼────┼────┤             │
│ │    │    │    │    │             │
│ │    │    │    │    │             │
│ └────┴────┴────┴────┘             │
│                                    │
│ ● Optimal  ● Warning  ● Critical   │ ← Legend
│                                    │
├────────────────────────────────────┤
│ Zones Needing Attention            │
│                                    │
│ 1. B3 - 28% CRITICAL               │ ← Priority list
│ 2. A3 - 38% Warning                │
│ 3. C2 - 42% Warning                │
│                                    │
├────────────────────────────────────┤
│  [Overview]    [Map]    [History]  │
└────────────────────────────────────┘

When zone tapped → Drawer opens:

┌────────────────────────────────────┐
│                                    │
│         ════════                   │
│                              [×]   │ ← Close button
├────────────────────────────────────┤
│ Zone B3                            │
│ Sensor: 0x4F2A                     │
│                                    │
│ Battery                            │
│ [████████░░░░] 67%                 │
│                                    │
│ ┌──────────┬──────────┐           │
│ │ Moisture │ Soil Temp│           │
│ │   28%    │   24°C   │           │
│ │ CRITICAL │  Normal  │           │
│ └──────────┴──────────┘           │
│                                    │
│ Last reading: 1 min ago            │
│                                    │
│ [View History]                     │
│                                    │
└────────────────────────────────────┘
```

### **12.4 Phase 1: History Page**

```
┌────────────────────────────────────┐
│ ■ AGRISCAN          [●] 2 min ago  │
├────────────────────────────────────┤
│                                    │
│ [24h] [7d] [30d]    [💧] [🌡]      │ ← Time range + metric toggles
│                                    │
│ Zone: [All Zones      ▼]           │ ← Zone filter dropdown
│                                    │
├────────────────────────────────────┤
│ Soil Moisture - Last 24 Hours      │
│                                    │
│ ┌────────────────────────────────┐ │
│ │70%                    ┌──────┐│ │
│ │    ╱╲      ╱╲        │ 45%  ││ │ ← Touch tooltip
│ │   ╱  ╲    ╱  ╲     ● │2:30pm││ │
│ │  ╱    ╲──╱    ╲───╱  └──────┘│ │
│ │30%                            │ │
│ │                               │ │
│ │ 12am   6am   12pm   6pm  Now │ │
│ └────────────────────────────────┘ │
│                                    │
├────────────────────────────────────┤
│ Summary                            │
│                                    │
│  High      Low       Average       │
│  68%       42%       55%           │
│  at 2pm    at 6am                  │
│                                    │
├────────────────────────────────────┤
│  [Overview]    [Map]    [History]  │
└────────────────────────────────────┘
```

---

## **13. File Structure**

```
/agriscan-dashboard/
│
├── index.html              # Single HTML file (all phases)
│
├── css/
│   └── main.css            # Complete stylesheet
│
├── js/
│   ├── app.js              # Main application logic
│   ├── api.js              # API fetch functions
│   ├── chart.js            # SVG chart renderer
│   ├── zones.js            # Zone grid + drawer (Phase 1)
│   └── utils.js            # Calculations, formatting
│
├── strings/
│   ├── en.json             # English strings
│   └── [locale].json       # Future translations
│
├── sw.js                   # Service worker for offline
│
└── manifest.json           # PWA manifest
```

---

## **14. Implementation Checklist**

### **Phase 0 Deliverables**
- [ ] Single-page HTML structure
- [ ] CSS variables and base styles
- [ ] LED indicator with pulse animation
- [ ] Metric card component (2 instances)
- [ ] API polling (10-second interval)
- [ ] Status color logic
- [ ] Timestamp formatting
- [ ] Basic error state handling
- [ ] Onboarding overlay (3 steps)
- [ ] Service worker registration
- [ ] Warm color palette applied
- [ ] All text ≥14px verified

### **Phase 1 Deliverables**
- [ ] Bottom tab navigation
- [ ] Overview page layout
- [ ] Map page with 4x4 zone grid
- [ ] History page with chart
- [ ] Zone drawer component
- [ ] Toggle button groups
- [ ] SVG chart with touch tooltip
- [ ] Priority action cards
- [ ] Multi-zone API integration
- [ ] Zone filtering for history
- [ ] Loading skeleton states
- [ ] ARIA landmarks implemented
- [ ] Landscape orientation support

### **Phase 2 Deliverables**
- [ ] Settings page
- [ ] Extended history (30 days)
- [ ] CSV export functionality
- [ ] Sensor diagnostics view
- [ ] Alert threshold customization
- [ ] Calibration helper
- [ ] Weather integration (optional)

---

## **15. Quality Checklist (Pre-Launch)**

### **Accessibility**
- [ ] All text ≥14px
- [ ] All touch targets ≥48px
- [ ] Color contrast AA compliant
- [ ] ARIA landmarks present
- [ ] Screen reader tested
- [ ] Keyboard navigation works
- [ ] Focus indicators visible
- [ ] Reduced motion respected

### **Performance**
- [ ] Total bundle <80KB gzipped
- [ ] First paint <500ms
- [ ] Interactive <1 second
- [ ] Offline mode works
- [ ] Service worker caches correctly
- [ ] Memory stable over 30 minutes

### **Device Testing**
- [ ] iPhone SE (320px)
- [ ] iPhone 12/13/14 (390px)
- [ ] Pixel 5 (393px)
- [ ] Samsung A-series (various)
- [ ] Budget Android (Tecno/Infinix)
- [ ] Tablet 10" portrait
- [ ] Desktop 1920px

### **Content**
- [ ] All strings externalized
- [ ] Terminology consistent
- [ ] Error messages helpful
- [ ] Status messages clear
- [ ] Timestamps localized

---

*End of Specification v2.0*
