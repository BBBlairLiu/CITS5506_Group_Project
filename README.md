# SmartSan IoT Group Project

SmartSan is an IoT-based hand sanitiser monitoring system designed for CITS5506.  
The current prototype design combines automatic dispensing, sanitizer level monitoring, and a simple browser-based monitoring interface.

## Current Repository Status

This repository currently includes an initial **mock dashboard page** for the SmartSan system.

At this stage:
- the dashboard is built with HTML, CSS, and JavaScript
- the displayed values are **mock data**
- the page is intended to simulate the future monitoring workflow before hardware arrives

## Current Design Overview

The current SmartSan design is based on:
- **ESP32** as the main controller
- **IR sensor** for hand detection
- **servo motor** for bottle pressing
- **load cell + HX711** for sanitizer level monitoring
- **Wi-Fi browser interface** for status display

The current software flow is planned as:

IR detects hand -> servo presses bottle -> usage count updates -> weight is read and stabilised -> refill status is checked -> status is sent to the web page

## Dashboard Features (Mock Version)

The current dashboard shows:
- usage count
- current weight
- sanitizer status
- device state
- refill threshold
- last updated time

It also includes a **Simulate 1 Dispense** button to preview the expected monitoring workflow.

## How to Run

Open the dashboard file in a browser.

### Option 1
Open the HTML file directly:
- `src/smartsan_dashboard.html`

### Option 2
Use **Live Server** in VS Code:
1. Open the project in VS Code
2. Right-click the HTML file
3. Select **Open with Live Server**

## Project Structure

```text
CITS5506_GROUP_PROJECT/
├─ src/
│  └─ smartsan_dashboard.html
└─ README.md