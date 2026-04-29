# SmartSan IoT Group Project

SmartSan is an IoT-based hand sanitiser monitoring system designed for CITS5506.  
The current prototype design combines automatic dispensing, sanitizer level monitoring, and a simple browser-based monitoring interface.

## Current Repository Status

This repository currently includes a mock dashboard, software planning documents, and an ESP32 firmware skeleton for the SmartSan system.

At this stage:
- the dashboard is built with HTML, CSS, and JavaScript
- the displayed values are **mock data following the planned ESP32/Blynk data contract**
- the firmware can run with simulated hardware while the real components are in transit
- the page is intended to simulate the future monitoring workflow before hardware arrives

## Current Design Overview

The current SmartSan design is based on:
- **ESP32** as the main controller
- **IR sensor** for hand detection
- **servo motor** for bottle pressing
- **pump-count based refill estimation** for the current software prototype
- **Blynk dashboard** as the primary mobile dashboard
- **local browser interface** as a mock/demo fallback

The current software flow is planned as:

IR detects hand -> servo presses bottle -> usage count updates -> remaining pump count is updated -> refill status is checked -> status is sent to Blynk/dashboard

## Dashboard Features (Mock Version)

The current dashboard shows:
- usage count
- remaining pumps
- remaining level percentage
- sanitizer status
- device state
- device online state
- refill threshold (pump count)
- last dispense time
- sync status and last sync time
- last updated time

It also includes demo controls for simulated sensor dispense, manual dispense, alert reset, system enable/disable, simulated sync failure, and an event timeline.

## Firmware Skeleton

The ESP32 Arduino sketch is available at:

- `firmware/SmartSanESP32/SmartSanESP32.ino`

By default, the sketch uses mock hardware mode:

- Serial Monitor input `d` simulates a valid hand-detection event
- Blynk virtual pins are updated using the planned dashboard data contract
- hardware adapter functions are isolated so they can later be replaced with real IR, servo, and HX711 logic

Before uploading to an ESP32, replace the placeholder Blynk and Wi-Fi values in the sketch.

## Software Documents

- `docs/software_functionalities.md` summarises the software features.
- `docs/blynk_dashboard.md` defines the Blynk dashboard widgets and virtual pins.
- `docs/data_contract.md` defines the fields shared between firmware and dashboard.
- `docs/integration_test_plan.md` lists the hardware integration and evidence checklist.
- `docs/software_test_checklist.md` lists software-only validation scenarios before hardware integration.

## How to Run

Open the dashboard file in a browser.

### Option 1
Open the HTML file directly:
- `src/index.html`

### Option 2
Use **Live Server** in VS Code:
1. Open the project in VS Code
2. Right-click the HTML file
3. Select **Open with Live Server**

## Project Structure

```text
CITS5506_GROUP_PROJECT/
├─ docs/
│  ├─ blynk_dashboard.md
│  ├─ data_contract.md
│  ├─ integration_test_plan.md
│  ├─ software_test_checklist.md
│  └─ software_functionalities.md
├─ firmware/
│  └─ SmartSanESP32/
│     └─ SmartSanESP32.ino
├─ src/
│  └─ index.html
└─ README.md