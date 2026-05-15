/*
  SmartSan ESP32 firmware.

  Mock mode keeps the original Serial Monitor test path. Real hardware mode uses
  the hardware group's VL53L1X distance sensor, HX711 load cell, and servo logic.
*/

// Blynk configuration
#define BLYNK_TEMPLATE_NAME "SmartSan Prototype"

// Copy secrets.example.h to secrets.h and fill in local credentials before upload.
// secrets.h is intentionally ignored by git so Blynk tokens and Wi-Fi passwords
// are not pushed to the remote repository.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define BLYNK_TEMPLATE_ID "REPLACE_WITH_TEMPLATE_ID"
#define BLYNK_AUTH_TOKEN "REPLACE_WITH_DEVICE_AUTH_TOKEN"
const char WIFI_SSID[] = "REPLACE_WITH_WIFI_NAME";
const char WIFI_PASS[] = "REPLACE_WITH_WIFI_PASSWORD";
#endif

// Set to 0 after the hardware wiring and Arduino libraries are ready.
#define USE_MOCK_HARDWARE 1

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#if !USE_MOCK_HARDWARE
#include <Wire.h>
#include <VL53L1X.h>
#include <ESP32Servo.h>
#include "HX711.h"
#endif

// Hardware pin map from project_iot_adjustment.ino.
// D pin labels are used by the selected XIAO ESP32 board package in Arduino IDE.
#if !USE_MOCK_HARDWARE
const int PIN_SERVO = D1;
const int PIN_LOADCELL_DOUT = D2;
const int PIN_LOADCELL_SCK = D3;
const int PIN_DISTANCE_SDA = D4;
const int PIN_DISTANCE_SCL = D5;
#endif

// Distance trigger settings
const int MIN_HAND_DISTANCE_MM = 70;
const int MAX_HAND_DISTANCE_MM = 150;
const int DISTANCE_OFFSET_MM = 5;

// Servo settings from the hardware integration sketch
const int SERVO_HOME_DEG = 90;
const int SERVO_PRESS_DEG = 15;
const int SERVO_STEP_DELAY_MS = 35;
const int SERVO_HOLD_MS = 700;
const int SERVO_RETURN_SETTLE_MS = 1000;

// HX711 load cell calibration from the hardware group.
// Empty bottle is treated as 0 g liquid weight.
const long LOADCELL_ZERO_RAW = 1045992;
const float LOADCELL_CALIBRATION_FACTOR = 2153.3;
const float FULL_LIQUID_GRAMS = 600.0;
const float EMPTY_NOISE_GRAMS = 5.0;
const int REFILL_ALERT_PERCENT = 10;

// Blynk virtual pin definitions
const int PIN_USAGE_COUNT = V0;
const int PIN_REMAINING_PERCENT = V2;
const int PIN_REFILL_ALERT = V3;
const int PIN_DEVICE_STATE = V4;
const int PIN_LAST_DISPENSE_AT = V5;
const int PIN_DEVICE_ONLINE = V6;
const int PIN_LIQUID_WEIGHT_GRAMS = V7;
const int PIN_DISTANCE_MM = V8;
const int PIN_LAST_DISPENSE_GRAMS = V9;
const int PIN_MANUAL_DISPENSE = V10;
const int PIN_RESET_ALERT = V11;
const int PIN_SYSTEM_ENABLED = V12;

// Device and state machine variables
const int MAX_PUMPS = 25;
const unsigned long DISPENSE_LOCKOUT_MS = 2000;
const unsigned long STABILISE_DELAY_MS = 1200;
const unsigned long STATUS_INTERVAL_MS = 5000;
const unsigned long MEASUREMENT_INTERVAL_MS = 500;
const unsigned long BLYNK_RECONNECT_INTERVAL_MS = 10000;
const unsigned long STABILISE_TIMEOUT_MS = 5000;
const unsigned long ERROR_RECOVERY_MS = 8000;

enum DeviceState {
  STATE_IDLE,
  STATE_HAND_DETECTED,
  STATE_DISPENSING,
  STATE_WAIT_STABILISE,
  STATE_CHECK_REFILL,
  STATE_UPDATE_STATUS,
  STATE_REFILL_REQUIRED,
  STATE_DISABLED,
  STATE_ERROR
};

DeviceState deviceState = STATE_IDLE;

#if !USE_MOCK_HARDWARE
VL53L1X distanceSensor;
Servo dispenserServo;
HX711 scale;
#endif

int usageCount = 0;
int sessionCount = 0;
int remainingPumpsEstimate = MAX_PUMPS;
int remainingPercent = 100;
bool refillAlert = false;
bool systemEnabled = true;
bool manualDispenseRequested = false;
bool resetAlertRequested = false;
bool dispensingActive = false;
bool handPresent = false;
bool distanceSensorReady = false;
bool scaleReady = false;

long loadCellRaw = 0;
float liquidWeightGrams = FULL_LIQUID_GRAMS;
float previousLiquidWeightGrams = FULL_LIQUID_GRAMS;
float lastDispenseGrams = 0.0;
int distanceRawMm = 0;
int distanceMm = 0;

unsigned long lastDispenseMs = 0;
unsigned long stateStartedMs = 0;
unsigned long lastStatusMs = 0;
unsigned long lastMeasurementMs = 0;
unsigned long lastErrorLogMs = 0;
unsigned long lastBlynkReconnectMs = 0;

bool csvHeaderPrinted = false;

String stateToString(DeviceState state) {
  switch (state) {
    case STATE_IDLE:
      return "IDLE";
    case STATE_HAND_DETECTED:
      return "HAND_DETECTED";
    case STATE_DISPENSING:
      return "DISPENSING";
    case STATE_WAIT_STABILISE:
      return "WAIT_STABILISE";
    case STATE_CHECK_REFILL:
      return "CHECK_REFILL";
    case STATE_UPDATE_STATUS:
      return "UPDATE_STATUS";
    case STATE_REFILL_REQUIRED:
      return "REFILL_REQUIRED";
    case STATE_DISABLED:
      return "DISABLED";
    case STATE_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN_STATE";
  }
}

float clampFloat(float value, float lower, float upper) {
  if (value < lower) {
    return lower;
  }

  if (value > upper) {
    return upper;
  }

  return value;
}

void updateRemainingMetrics() {
  float percent = (liquidWeightGrams / FULL_LIQUID_GRAMS) * 100.0;
  remainingPercent = (int)(clampFloat(percent, 0.0, 100.0) + 0.5);

  float pumpEstimate = (remainingPercent / 100.0) * MAX_PUMPS;
  remainingPumpsEstimate = (int)(clampFloat(pumpEstimate, 0.0, MAX_PUMPS) + 0.5);
}

void updateRefillAlert() {
  refillAlert = remainingPercent <= REFILL_ALERT_PERCENT;
}

void printCsvHeaderOnce() {
  if (csvHeaderPrinted) {
    return;
  }

  Serial.println("time_ms,distance_mm,load_raw,liquid_g,remaining_percent,last_dispense_g,state,event");
  csvHeaderPrinted = true;
}

void printCsvSample(const char* eventName) {
  printCsvHeaderOnce();
  Serial.print(millis());
  Serial.print(",");
  Serial.print(distanceMm);
  Serial.print(",");
  Serial.print(loadCellRaw);
  Serial.print(",");
  Serial.print(liquidWeightGrams, 1);
  Serial.print(",");
  Serial.print(remainingPercent);
  Serial.print(",");
  Serial.print(lastDispenseGrams, 1);
  Serial.print(",");
  Serial.print(stateToString(deviceState));
  Serial.print(",");
  Serial.println(eventName);
}

void connectBlynk() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Blynk.config(BLYNK_AUTH_TOKEN);

  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);

  if (Blynk.connect(5000)) {
    Serial.println("[BLYNK] Connected");
  } else {
    Serial.println("[BLYNK] Not connected yet; local Serial testing will continue");
  }
}

void runBlynkConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!Blynk.connected() && millis() - lastBlynkReconnectMs >= BLYNK_RECONNECT_INTERVAL_MS) {
    lastBlynkReconnectMs = millis();
    Blynk.connect(1000);
  }

  if (Blynk.connected()) {
    Blynk.run();
  }
}

#if !USE_MOCK_HARDWARE
void setupHardware() {
  Wire.begin(PIN_DISTANCE_SDA, PIN_DISTANCE_SCL);

  distanceSensor.setTimeout(500);
  if (!distanceSensor.init()) {
    Serial.println("[ERROR] Distance sensor failed");
    deviceState = STATE_ERROR;
  } else {
    distanceSensor.startContinuous(50);
    distanceSensorReady = true;
    Serial.println("[HW] Distance sensor started");
  }

  dispenserServo.attach(PIN_SERVO);
  dispenserServo.write(SERVO_HOME_DEG);
  delay(500);
  Serial.println("[HW] Servo ready");

  scale.begin(PIN_LOADCELL_DOUT, PIN_LOADCELL_SCK);
  if (!scale.is_ready()) {
    Serial.println("[ERROR] HX711 not found. Check DT/SCK wiring.");
    deviceState = STATE_ERROR;
  } else {
    scaleReady = true;
    Serial.println("[HW] HX711 ready");
  }
}

bool readDistanceMeasurement() {
  if (!distanceSensorReady) {
    return false;
  }

  distanceRawMm = distanceSensor.read();

  if (distanceSensor.timeoutOccurred()) {
    Serial.println("[WARN] Distance sensor timeout");
    return false;
  }

  distanceMm = distanceRawMm - DISTANCE_OFFSET_MM;
  if (distanceMm < 0) {
    distanceMm = 0;
  }

  return true;
}

bool readWeightMeasurement() {
  if (!scaleReady) {
    return false;
  }

  if (!scale.is_ready()) {
    Serial.println("[WARN] HX711 not ready");
    scaleReady = false;
    return false;
  }

  loadCellRaw = scale.read_average(10);
  liquidWeightGrams = (loadCellRaw - LOADCELL_ZERO_RAW) / LOADCELL_CALIBRATION_FACTOR;

  if (liquidWeightGrams > -EMPTY_NOISE_GRAMS && liquidWeightGrams < EMPTY_NOISE_GRAMS) {
    liquidWeightGrams = 0.0;
  }

  if (liquidWeightGrams < 0.0) {
    liquidWeightGrams = 0.0;
  }

  return true;
}
#endif

void sampleMeasurements(const char* eventName) {
#if USE_MOCK_HARDWARE
  liquidWeightGrams = (remainingPumpsEstimate * FULL_LIQUID_GRAMS) / MAX_PUMPS;
  loadCellRaw = 0;
  distanceRawMm = 0;
  distanceMm = 0;
#else
  readDistanceMeasurement();
  readWeightMeasurement();
#endif

  updateRemainingMetrics();
  updateRefillAlert();
  printCsvSample(eventName);
}

void sampleMeasurementsIfDue() {
  if (millis() - lastMeasurementMs < MEASUREMENT_INTERVAL_MS) {
    return;
  }

  lastMeasurementMs = millis();
  sampleMeasurements("sample");
}

void sendStatus() {
  updateRemainingMetrics();
  updateRefillAlert();

  if (!Blynk.connected()) {
    return;
  }

  Blynk.virtualWrite(PIN_USAGE_COUNT, usageCount);
  Blynk.virtualWrite(PIN_REMAINING_PERCENT, remainingPercent);
  Blynk.virtualWrite(PIN_REFILL_ALERT, refillAlert ? 1 : 0);
  Blynk.virtualWrite(PIN_DEVICE_STATE, stateToString(deviceState));
  Blynk.virtualWrite(PIN_LAST_DISPENSE_AT, lastDispenseMs == 0 ? "--" : String(lastDispenseMs / 1000) + "s");
  Blynk.virtualWrite(PIN_DEVICE_ONLINE, 1);
  Blynk.virtualWrite(PIN_LIQUID_WEIGHT_GRAMS, liquidWeightGrams);
  Blynk.virtualWrite(PIN_DISTANCE_MM, distanceMm);
  Blynk.virtualWrite(PIN_LAST_DISPENSE_GRAMS, lastDispenseGrams);
}

void setState(DeviceState nextState) {
  deviceState = nextState;
  stateStartedMs = millis();
  sendStatus();
}

bool hasLiquidAvailable() {
  return remainingPercent > REFILL_ALERT_PERCENT;
}

bool hardwareReadyForDispense() {
#if USE_MOCK_HARDWARE
  return true;
#else
  return distanceSensorReady && scaleReady;
#endif
}

bool canStartDispense() {
  if (dispensingActive) {
    Serial.println("[GUARD] Dispense already active");
    return false;
  }

  if (!systemEnabled) {
    return false;
  }

  if (millis() - lastDispenseMs < DISPENSE_LOCKOUT_MS) {
    return false;
  }

  if (!hardwareReadyForDispense()) {
    Serial.println("[BLOCK] Hardware not ready");
    return false;
  }

  if (!hasLiquidAvailable()) {
    return false;
  }

  return deviceState == STATE_IDLE || deviceState == STATE_REFILL_REQUIRED;
}

void startDispenseCycle() {
  if (!systemEnabled) {
    setState(STATE_DISABLED);
    return;
  }

  if (!canStartDispense()) {
    if (!hardwareReadyForDispense()) {
      setState(STATE_ERROR);
    } else if (!hasLiquidAvailable()) {
      Serial.println("[BLOCK] Refill required");
      setState(STATE_REFILL_REQUIRED);
    }

    return;
  }

  dispensingActive = true;
  setState(STATE_HAND_DETECTED);
}

void updateStateMachine() {
  switch (deviceState) {
    case STATE_IDLE:
    case STATE_REFILL_REQUIRED:
      if (manualDispenseRequested) {
        manualDispenseRequested = false;
        startDispenseCycle();
      } else if (readHandDetected()) {
        startDispenseCycle();
      }
      break;

    case STATE_HAND_DETECTED:
      performDispense();
      setState(STATE_DISPENSING);
      break;

    case STATE_DISPENSING:
      usageCount += 1;
      sessionCount += 1;

#if USE_MOCK_HARDWARE
      if (remainingPumpsEstimate > 0) {
        remainingPumpsEstimate -= 1;
      }
      liquidWeightGrams = (remainingPumpsEstimate * FULL_LIQUID_GRAMS) / MAX_PUMPS;
      lastDispenseGrams = FULL_LIQUID_GRAMS / MAX_PUMPS;
#endif

      updateRemainingMetrics();
      lastDispenseMs = millis();
      dispensingActive = false;

      Serial.print("[COUNT] Total: ");
      Serial.print(usageCount);
      Serial.print(" | Session: ");
      Serial.print(sessionCount);
      Serial.print(" | Remaining %: ");
      Serial.print(remainingPercent);
      Serial.print(" | Liquid g: ");
      Serial.println(liquidWeightGrams, 1);

      setState(STATE_WAIT_STABILISE);
      break;

    case STATE_WAIT_STABILISE:
      if (millis() - stateStartedMs >= STABILISE_DELAY_MS) {
        setState(STATE_CHECK_REFILL);
        break;
      }

      if (millis() - stateStartedMs >= STABILISE_TIMEOUT_MS) {
        Serial.println("[ERROR] WAIT_STABILISE timeout");
        setState(STATE_ERROR);
      }
      break;

    case STATE_CHECK_REFILL:
#if !USE_MOCK_HARDWARE
      sampleMeasurements("post_weight");
      lastDispenseGrams = previousLiquidWeightGrams - liquidWeightGrams;
      if (lastDispenseGrams < 0.0) {
        lastDispenseGrams = 0.0;
      }
      printCsvSample("post_dispense");
#endif
      updateRemainingMetrics();
      updateRefillAlert();
      setState(STATE_UPDATE_STATUS);
      break;

    case STATE_UPDATE_STATUS:
      setState(refillAlert ? STATE_REFILL_REQUIRED : STATE_IDLE);
      break;

    case STATE_DISABLED:
      dispensingActive = false;

      if (systemEnabled) {
        setState(refillAlert ? STATE_REFILL_REQUIRED : STATE_IDLE);
      }
      break;

    case STATE_ERROR:
      if (millis() - lastErrorLogMs >= 2000) {
        lastErrorLogMs = millis();
        Serial.println("[ERROR] System in STATE_ERROR");
      }

      if (millis() - stateStartedMs >= ERROR_RECOVERY_MS) {
        Serial.println("[RECOVERY] Attempting system recovery");
        dispensingActive = false;
        sampleMeasurements("recovery");

        if (hardwareReadyForDispense()) {
          setState(systemEnabled ? STATE_IDLE : STATE_DISABLED);
        } else {
          Serial.println("[RECOVERY] Hardware still not ready");
          stateStartedMs = millis();
        }
      }
      break;
  }

  if (resetAlertRequested) {
    resetAlertRequested = false;

#if USE_MOCK_HARDWARE
    remainingPumpsEstimate = MAX_PUMPS;
    liquidWeightGrams = FULL_LIQUID_GRAMS;
#else
    sampleMeasurements("reset");
#endif

    lastDispenseGrams = 0.0;
    updateRemainingMetrics();
    updateRefillAlert();
    dispensingActive = false;

    Serial.println("[RESET] Refill reset requested");
    setState(systemEnabled ? (refillAlert ? STATE_REFILL_REQUIRED : STATE_IDLE) : STATE_DISABLED);
  }
}

BLYNK_WRITE(V10) {
  manualDispenseRequested = param.asInt() == 1;
}

BLYNK_WRITE(V11) {
  resetAlertRequested = param.asInt() == 1;
}

BLYNK_WRITE(V12) {
  systemEnabled = param.asInt() == 1;
  setState(systemEnabled ? (refillAlert ? STATE_REFILL_REQUIRED : STATE_IDLE) : STATE_DISABLED);
}

void setup() {
  Serial.begin(115200);
  delay(100);

#if !USE_MOCK_HARDWARE
  setupHardware();
#else
  Serial.println("[MOCK] Hardware simulation enabled");
#endif

  sampleMeasurements("boot");
  connectBlynk();

  dispensingActive = false;
  setState(deviceState == STATE_ERROR ? STATE_ERROR : STATE_IDLE);
}

void loop() {
  runBlynkConnection();
  sampleMeasurementsIfDue();
  updateStateMachine();

  if (millis() - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = millis();
    sendStatus();
  }
}

bool readHandDetected() {
#if USE_MOCK_HARDWARE
  if (Serial.available() == 0) {
    return false;
  }

  char command = Serial.read();

  if (command == 'd' || command == 'D') {
    Serial.println("[MOCK] Trigger dispense");
    return true;
  }

  return false;
#else
  if (!readDistanceMeasurement()) {
    return false;
  }

  bool inRange = distanceMm >= MIN_HAND_DISTANCE_MM && distanceMm <= MAX_HAND_DISTANCE_MM;

  if (inRange && !handPresent) {
    handPresent = true;
    Serial.print("[DISTANCE] Hand detected at ");
    Serial.print(distanceMm);
    Serial.println(" mm");
    return true;
  }

  if (!inRange) {
    handPresent = false;
  }

  return false;
#endif
}

void performDispense() {
#if USE_MOCK_HARDWARE
  Serial.println("[MOCK] Dispense start");
  delay(200);
  Serial.println("[MOCK] Dispense end");
#else
  readWeightMeasurement();
  previousLiquidWeightGrams = liquidWeightGrams;

  Serial.println("[SERVO] Press started");

  for (int angle = SERVO_HOME_DEG; angle >= SERVO_PRESS_DEG; angle--) {
    dispenserServo.write(angle);
    delay(SERVO_STEP_DELAY_MS);
  }

  delay(SERVO_HOLD_MS);

  for (int angle = SERVO_PRESS_DEG; angle <= SERVO_HOME_DEG; angle++) {
    dispenserServo.write(angle);
    delay(SERVO_STEP_DELAY_MS);
  }

  delay(SERVO_RETURN_SETTLE_MS);
  Serial.println("[SERVO] Cycle complete");
#endif
}
