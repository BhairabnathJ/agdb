# **Complete Feature List by Tier**

---

## **TIER 1: Simple Mode (Default View)**

### **Display Features:**
1. Status icon (🔴/🟡/🟢 emoji or colored circle)
2. Primary action message ("WATER YOUR CROPS" / "CHECK FIELD SOON" / "ALL GOOD")
3. Specific instruction ("Open valve for 2 hours" / "No action needed")
4. "Why?" button → navigates to Tier 2
5. "Show Details" button → navigates to Tier 3
6. Timestamp ("Last updated: X seconds ago")
7. Language selector (first-time only, then hidden)

### **Behind-the-Scenes Features:**
8. Auto-refresh every 10 seconds
9. Fetch sensor data from ESP32 API
10. Run physics engine (VPD, ET₀, drying rate calculations)
11. Generate decision (IRRIGATE_NOW / CHECK_SOON / ALL_GOOD)
12. Load translations from language JSON
13. Calculate actionable instruction (hours to irrigate)

---

## **TIER 2: Reasoning Mode (Why View)**

### **Display Features:**
14. "Back" button → returns to Tier 1
15. Question heading ("Why do I need to water?")
16. 3-5 bullet points explaining reasoning:
    - Current moisture level + comparison to threshold
    - Drying rate (how fast moisture is dropping)
    - Time until crop stress begins
    - Consequence of not acting (yield impact %)
    - Plant stress indicators (VPD-related)
17. "OK, Got It" button → returns to Tier 1
18. "Show Technical Details" button → jumps to Tier 3

### **Behind-the-Scenes Features:**
19. Generate plain-language explanations from decision object
20. Dynamic reasoning based on action type (irrigate/check/good)
21. Pull consequence data from crop profile

---

## **TIER 3: Expert Mode (Technical View)**

### **Display Features:**

**Navigation:**
22. "< Back to Simple View" button → returns to Tier 1

**Current Readings Grid:**
23. Soil moisture (%)
24. Soil temperature (°C)
25. Air temperature (°C)
26. Air humidity (%)
27. VPD (kPa) - calculated
28. EC / Soil salinity (mS/cm)
29. Leaf wetness (0-100 or dry/wet)
30. Color-coded borders on each metric (red/yellow/green based on status)

**Calculated Metrics:**
31. Drying rate (%/hour)
32. ET₀ - Reference evapotranspiration (mm/day)
33. Time to critical threshold (hours)
34. Disease risk level (LOW/MODERATE/HIGH)
35. Nutrient status (based on EC reading)
36. Root zone depletion (%)
37. Water balance (mm surplus/deficit)

**24-Hour History Chart:**
38. Line chart with dual Y-axis (moisture + temperature)
39. 144 data points (10-minute intervals)
40. Shaded "healthy zone" band (50-70% moisture)
41. Hover tooltips showing exact values at timestamp
42. Zoom/pan controls (optional)
43. Irrigation event markers (vertical lines where farmer watered)
44. Critical threshold line (horizontal line at 30%)

**Recommendation Basis:**
45. Technical paragraph explaining decision logic
46. Shows which factors triggered recommendation
47. References crop stage and thresholds
48. Explains VPD impact on plant stress
49. Shows water deficit calculation

**Action Buttons:**
50. "Download Data CSV" → exports sensor history
51. "View Settings" → opens settings page (if exists)

**Multi-Zone Features (if applicable):**
52. Zone selector dropdown (filter chart to specific zone)
53. Multi-zone overlay toggle (show all zones on one chart)
54. Zone comparison table

### **Behind-the-Scenes Features:**
55. Fetch 24-hour history from ESP32 (or from local cache)
56. Render Chart.js line graph
57. Format all technical values (decimal places, units)
58. Generate technical reasoning paragraph
59. Prepare CSV export data

---

## **GLOBAL FEATURES (All Tiers)**

### **Language System:**
60. Load selected language JSON on startup
61. Translation function for all text (t() function)
62. Support for parameter replacement ({hours}, {moisture}, etc.)
63. Language persistence (saves choice to localStorage)
64. RTL support for Arabic/Hebrew (future)

### **Offline Support:**
65. All assets cached in browser (no CDN dependencies)
66. Works without internet connection
67. Displays "Offline" indicator if ESP32 disconnected
68. Graceful degradation (shows last known data if connection lost)

### **Responsive Design:**
69. Mobile-first layout (320px minimum width)
70. Scales to tablet and desktop
71. Touch-friendly button sizes (44px minimum)
72. Readable text in bright sunlight (high contrast)

### **Navigation:**
73. URL-based routing (optional: #tier1, #tier2, #tier3)
74. Browser back button support
75. Always easy to return to Tier 1 from anywhere

### **Performance:**
76. Page load time <1 second
77. Transitions between tiers <200ms
78. Chart render time <500ms
79. API response time <100ms

---

## **SETTINGS PAGE (Separate Page, Not a Tier)**

**Accessed from Tier 3 "View Settings" button**

### **Display Features:**
80. "Back" button → returns to Tier 3
81. Language selector dropdown (change language anytime)
82. Unit toggles:
    - Temperature: Celsius ↔ Fahrenheit
    - Volume: Liters ↔ Gallons
    - Area: Hectares ↔ Acres

**Quick Setup (Minimal Configuration):**
83. Crop type selector (Maize / Rice / Wheat / Vegetables / Custom)
84. Planting date picker (calendar widget)
85. "Done" button → saves and returns

**Advanced (Collapsed by Default):**
86. Custom moisture thresholds (critical, warning sliders)
87. Irrigation system flow rate (L/hour input)
88. Farm area (hectares/acres input)
89. Root depth (mm input)
90. Water cost per liter (for ROI calculations)

**Device Info (Read-Only):**
91. Device serial number
92. Firmware version
93. WiFi SSID and password (with "Change" button)
94. Storage used (MB of SD card)
95. Battery voltage (if battery-powered)

### **Behind-the-Scenes Features:**
96. Save all settings to ESP32 (JSON file on SD card)
97. Auto-adjust thresholds based on crop type
98. Calculate crop growth stage from planting date
99. Validate all inputs before saving

---

## **SUMMARY BY TIER**

| Tier | Feature Count | Complexity |
|------|---------------|------------|
| **Tier 1** | 13 features | Very Simple |
| **Tier 2** | 8 features | Simple |
| **Tier 3** | 38 features | Complex |
| **Global** | 16 features | Infrastructure |
| **Settings** | 20 features | Configuration |
| **TOTAL** | **95 features** | |

---

## **FEATURE PRIORITY (For Development)**

### **Phase 0 (MVP - Week 1-2):**
Features: 1-13 (Tier 1 only)
- Status icon, message, instruction
- Buttons (non-functional at first)
- Basic fetch from ESP32
- Simple threshold logic (moisture < 30% = red)

### **Phase 1 (Intelligence - Week 3-4):**
Features: 8-13, 19-21, 31-37
- Implement full physics engine
- VPD, ET₀, drying rate calculations
- Decision engine with reasoning

### **Phase 2 (Reasoning - Week 5):**
Features: 14-18
- Add Tier 2 view
- Generate plain-language explanations
- Navigation between Tier 1 and 2

### **Phase 3 (Expert View - Week 6-7):**
Features: 22-51
- Add Tier 3 view
- Implement Chart.js
- Technical metrics display
- CSV export

### **Phase 4 (Polish - Week 8):**
Features: 60-79
- Language system
- Offline support
- Responsive design refinement
- Performance optimization

### **Phase 5 (Settings - Week 9):**
Features: 80-99
- Settings page
- Quick setup wizard
- Advanced configuration

---

## **FEATURE DEPENDENCIES**

**Tier 1 requires:**
- ESP32 serving sensor data JSON
- Physics engine (physics.js)
- Language files
- Basic CSS

**Tier 2 requires:**
- Everything from Tier 1
- Reasoning generation logic

**Tier 3 requires:**
- Everything from Tier 1
- Chart.js library
- Historical data from ESP32
- CSV export logic

**Settings requires:**
- Persistent storage (SD card JSON)
- Form validation
- Crop profile database

---

**This is every feature, organized by where it appears. Clear?**