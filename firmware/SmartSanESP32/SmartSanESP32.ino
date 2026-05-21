/*
  SmartSan ESP32 firmware.

  Mock mode keeps the original Serial Monitor test path. Real hardware mode uses
  the hardware group's VL53L1X distance sensor, HX711 load cell, servo logic,
  and optional status LEDs.
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
#define USE_MOCK_HARDWARE 0

// Status LEDs: green/yellow/red show fill level, white shows hand detection.
#define ENABLE_STATUS_LEDS 1

// Temporary hardware bypass: keep this at 0 while HX711 wiring is being checked.
// Distance sensing, servo dispensing, Serial output, and Blynk updates still run.
#define ENABLE_HX711 1

// Temporary test bypass: set to 0 only while the distance sensor I2C issue is being debugged.
// Manual Blynk dispense still works; hand-triggered dispense is disabled.
#define ENABLE_DISTANCE_SENSOR 1

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#if !USE_MOCK_HARDWARE
#include <Wire.h>
#include <VL53L1X.h>
#include <ESP32Servo.h>
#if ENABLE_HX711
#include "HX711.h"
#endif
#endif

// Hardware pin map from project_iot_adjustment.ino.
// D pin labels are used by the selected XIAO ESP32 board package in Arduino IDE.
#if !USE_MOCK_HARDWARE
const int PIN_SERVO = D1;
#if ENABLE_HX711
const int PIN_LOADCELL_DOUT = D2;
const int PIN_LOADCELL_SCK = D3;
#endif
const int PIN_DISTANCE_SDA = D4;
const int PIN_DISTANCE_SCL = D5;
#endif

#if !USE_MOCK_HARDWARE && ENABLE_STATUS_LEDS
// Optional status LEDs. Keep D1-D5 reserved for servo, HX711, and VL53L1X.
const int PIN_LED_GREEN = D8;
const int PIN_LED_YELLOW = D7;
const int PIN_LED_RED = D9;
const int PIN_LED_WHITE = D0;
const bool LED_ACTIVE_HIGH = true;
const int LED_TEST_STEP_MS = 1500;
const int LED_TEST_OFF_STEP_MS = 2000;
#endif

// Distance trigger settings
const int MIN_HAND_DISTANCE_MM = 70;
const int MAX_HAND_DISTANCE_MM = 150;
const int DISTANCE_OFFSET_MM = 5;
const int DISTANCE_SENSOR_PERIOD_MS = 20;
const int DISTANCE_SENSOR_TIMEOUT_MS = 200;

// Servo settings from the hardware integration sketch
const int SERVO_HOME_DEG = 90;
const int SERVO_PRESS_DEG = 15;
const int SERVO_STEP_DELAY_MS = 8;
const int SERVO_HOLD_MS = 450;
const int SERVO_RETURN_SETTLE_MS = 250;

// HX711 load cell calibration from the hardware group.
// Empty bottle is treated as 0 g liquid weight.
const long LOADCELL_ZERO_RAW = 1045992;
const float LOADCELL_CALIBRATION_FACTOR = 2153.3;
const float FULL_LIQUID_GRAMS = 250.0;
const float EMPTY_NOISE_GRAMS = 5.0;
const int REFILL_ALERT_PERCENT = 25;
const int LED_RED_BELOW_PERCENT = 25;
const int LED_YELLOW_MAX_PERCENT = 50;

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
const int PIN_CALIBRATE_ZERO = V13;
const int PIN_CALIBRATE_FULL = V14;
const int PIN_LED_TEST = V15;
const int PIN_CALIBRATION_STATUS = V16;

// Device and state machine variables
const int MAX_PUMPS = 25;
const unsigned long DISPENSE_LOCKOUT_MS = 1000;
const unsigned long STABILISE_DELAY_MS = 700;
const unsigned long STATUS_INTERVAL_MS = 5000;
const unsigned long MEASUREMENT_INTERVAL_MS = 250;
const unsigned long BLYNK_RECONNECT_INTERVAL_MS = 10000;
const unsigned long WIFI_STATUS_LOG_INTERVAL_MS = 5000;
const unsigned long STABILISE_TIMEOUT_MS = 5000;
const unsigned long ERROR_RECOVERY_MS = 4000;
const unsigned long HX711_READY_TIMEOUT_MS = 5000;
const unsigned long HX711_READY_RETRY_MS = 100;
const unsigned long HX711_SAMPLE_TIMEOUT_MS = 300;
const int HX711_READ_SAMPLES = 3;
const int HX711_CALIBRATION_SAMPLES = 10;

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
#if ENABLE_HX711
HX711 scale;
#endif
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
long loadCellZeroRaw = LOADCELL_ZERO_RAW;
float loadCellCalibrationFactor = LOADCELL_CALIBRATION_FACTOR;
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
unsigned long lastWiFiStatusLogMs = 0;
int lastWiFiStatus = -1;
bool blynkConnectedLogged = false;

bool csvHeaderPrinted = false;
String calibrationStatusMessage = "Calibration not run";

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

void setupStatusLeds() {
#if !USE_MOCK_HARDWARE && ENABLE_STATUS_LEDS
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);

  digitalWrite(PIN_LED_GREEN, LED_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(PIN_LED_YELLOW, LED_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(PIN_LED_RED, LED_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(PIN_LED_WHITE, LED_ACTIVE_HIGH ? LOW : HIGH);
#endif
}

void writeStatusLed(int pin, bool on) {
#if !USE_MOCK_HARDWARE && ENABLE_STATUS_LEDS
  digitalWrite(pin, on == LED_ACTIVE_HIGH ? HIGH : LOW);
#else
  (void)pin;
  (void)on;
#endif
}

void setStatusLeds(bool greenOn, bool yellowOn, bool redOn, bool whiteOn) {
#if !USE_MOCK_HARDWARE && ENABLE_STATUS_LEDS
  writeStatusLed(PIN_LED_GREEN, greenOn);
  writeStatusLed(PIN_LED_YELLOW, yellowOn);
  writeStatusLed(PIN_LED_RED, redOn);
  writeStatusLed(PIN_LED_WHITE, whiteOn);
#else
  (void)greenOn;
  (void)yellowOn;
  (void)redOn;
  (void)whiteOn;
#endif
}

void updateStatusLeds() {
  bool whiteOn = handPresent;
  bool redOn = deviceState == STATE_ERROR || remainingPercent < LED_RED_BELOW_PERCENT;
  bool yellowOn = !redOn && remainingPercent >= LED_RED_BELOW_PERCENT && remainingPercent <= LED_YELLOW_MAX_PERCENT;
  bool greenOn = !redOn && remainingPercent > LED_YELLOW_MAX_PERCENT;

  setStatusLeds(greenOn, yellowOn, redOn, whiteOn);
}

void testStatusLeds() {
  Serial.println("[LED] Test sequence: green -> yellow -> red -> white");
  setStatusLeds(false, false, false, false);
  delay(500);

  Serial.println("[LED] Testing green");
  setStatusLeds(true, false, false, false);
  delay(LED_TEST_STEP_MS);
  setStatusLeds(false, false, false, false);
  delay(500);

  Serial.println("[LED] Testing yellow");
  setStatusLeds(false, true, false, false);
  delay(LED_TEST_STEP_MS);
  setStatusLeds(false, false, false, false);
  delay(500);

  Serial.println("[LED] Testing red");
  setStatusLeds(false, false, true, false);
  delay(LED_TEST_STEP_MS);
  setStatusLeds(false, false, false, false);
  delay(500);

  Serial.println("[LED] Testing white");
  setStatusLeds(false, false, false, true);
  delay(LED_TEST_STEP_MS);
  setStatusLeds(false, false, false, false);
  delay(500);

  setStatusLeds(false, false, false, false);
  delay(300);
  updateRemainingMetrics();
  updateRefillAlert();
  updateStatusLeds();
  Serial.println("[LED] Test complete; restored normal status LEDs");
}

void forceStatusLedCommand(char command) {
  if (command == 'o' || command == 'O') {
    Serial.println("[LED] Force all off");
    setStatusLeds(false, false, false, false);
  } else if (command == 'g' || command == 'G') {
    Serial.println("[LED] Force green only");
    setStatusLeds(true, false, false, false);
  } else if (command == 'y' || command == 'Y') {
    Serial.println("[LED] Force yellow only");
    setStatusLeds(false, true, false, false);
  } else if (command == 'r' || command == 'R') {
    Serial.println("[LED] Force red only");
    setStatusLeds(false, false, true, false);
  } else if (command == 'w' || command == 'W') {
    Serial.println("[LED] Force white only");
    setStatusLeds(false, false, false, true);
  }
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

void publishCalibrationStatus(const String& message) {
  calibrationStatusMessage = message;

  Serial.print("[CAL] ");
  Serial.println(message);

  if (Blynk.connected()) {
    Blynk.virtualWrite(PIN_CALIBRATION_STATUS, message);
  }
}

void printWiFiStatus(const char* context) {
  int status = WiFi.status();

  Serial.print("[WIFI] ");
  Serial.print(context);
  Serial.print(" status: ");
  Serial.print(status);

  if (status == WL_CONNECTED) {
    Serial.print(" ip: ");
    Serial.print(WiFi.localIP());
  }

  Serial.println();
}

void reportWiFiStatus(const char* context, bool forceLog) {
  int status = WiFi.status();
  bool statusChanged = status != lastWiFiStatus;

  if (statusChanged) {
    lastWiFiStatus = status;
  }

  if (forceLog || statusChanged) {
    printWiFiStatus(context);
  }

  if (statusChanged && status == WL_CONNECTED) {
    Serial.println("[WIFI] Connected");
  }
}

void reportBlynkConnection() {
  if (Blynk.connected()) {
    if (!blynkConnectedLogged) {
      Serial.println("[BLYNK] Connected");
    }
    blynkConnectedLogged = true;
  } else {
    blynkConnectedLogged = false;
  }
}

void connectBlynk() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Blynk.config(BLYNK_AUTH_TOKEN);

  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);
  reportWiFiStatus("after begin", true);

  if (!Blynk.connect(5000)) {
    Serial.println("[BLYNK] Not connected yet; local Serial testing will continue");
  }
  reportBlynkConnection();
  reportWiFiStatus("after Blynk connect attempt", true);
}

void runBlynkConnection() {
  reportWiFiStatus("loop", false);

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiStatusLogMs >= WIFI_STATUS_LOG_INTERVAL_MS) {
      lastWiFiStatusLogMs = millis();
      printWiFiStatus("waiting");
    }
    return;
  }

  if (!Blynk.connected() && millis() - lastBlynkReconnectMs >= BLYNK_RECONNECT_INTERVAL_MS) {
    lastBlynkReconnectMs = millis();
    Blynk.connect(1000);
    reportBlynkConnection();
  }

  if (Blynk.connected()) {
    reportBlynkConnection();
    Blynk.run();
  } else {
    blynkConnectedLogged = false;
  }
}

#if !USE_MOCK_HARDWARE
bool waitForHx711Ready(unsigned long timeoutMs);

byte scanI2CPins(const char* label, int sdaPin, int sclPin) {
  byte foundCount = 0;

  Serial.print("[I2C] ");
  Serial.print(label);
  Serial.print(" SDA=GPIO");
  Serial.print(sdaPin);
  Serial.print(", SCL=GPIO");
  Serial.println(sclPin);

  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
  delay(20);
  Serial.print("[I2C] Line level SDA=");
  Serial.print(digitalRead(sdaPin));
  Serial.print(", SCL=");
  Serial.println(digitalRead(sclPin));

  Wire.end();
  delay(20);
  Wire.begin(sdaPin, sclPin);
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("[I2C] Found device at 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      foundCount += 1;
    }
  }

  return foundCount;
}

void scanI2CBus() {
  byte foundCount = 0;

  Serial.println("[I2C] Expected VL53L1X address is usually 0x29");
  foundCount += scanI2CPins("Configured bus D4/D5", PIN_DISTANCE_SDA, PIN_DISTANCE_SCL);

  if (foundCount == 0) {
    Serial.println("[I2C] Trying swapped D5/D4 in case SDA/SCL are reversed");
    foundCount += scanI2CPins("Swapped bus D5/D4", PIN_DISTANCE_SCL, PIN_DISTANCE_SDA);
  }

  if (foundCount == 0) {
    Serial.println("[I2C] No devices found on configured or swapped bus.");
    Serial.println("[I2C] SDA/SCL high means the bus is idle, but no sensor is ACKing.");
  }

  Wire.end();
  delay(20);
  Wire.begin(PIN_DISTANCE_SDA, PIN_DISTANCE_SCL);
}

bool beginDistanceSensorOnPins(const char* label, int sdaPin, int sclPin) {
  Serial.print("[HW] Trying distance sensor on ");
  Serial.print(label);
  Serial.print(" SDA=GPIO");
  Serial.print(sdaPin);
  Serial.print(", SCL=GPIO");
  Serial.println(sclPin);

  Wire.end();
  delay(20);
  Wire.begin(sdaPin, sclPin);
  distanceSensor.setTimeout(DISTANCE_SENSOR_TIMEOUT_MS);

  if (!distanceSensor.init()) {
    return false;
  }

  distanceSensor.startContinuous(DISTANCE_SENSOR_PERIOD_MS);
  distanceSensorReady = true;
  Serial.print("[HW] Distance sensor started on ");
  Serial.println(label);
  return true;
}

void setupHardware() {
#if ENABLE_DISTANCE_SENSOR
  if (!beginDistanceSensorOnPins("configured D4/D5", PIN_DISTANCE_SDA, PIN_DISTANCE_SCL)) {
    Serial.println("[ERROR] Distance sensor failed");
    scanI2CBus();

    if (!beginDistanceSensorOnPins("swapped D5/D4", PIN_DISTANCE_SCL, PIN_DISTANCE_SDA)) {
      Serial.println("[ERROR] Distance sensor failed on configured and swapped I2C pins");
      deviceState = STATE_ERROR;
    }
  }
#else
  distanceSensorReady = false;
  Serial.println("[HW] Distance sensor disabled; manual dispense testing only");
#endif

  dispenserServo.attach(PIN_SERVO);
  dispenserServo.write(SERVO_HOME_DEG);
  delay(500);
  Serial.println("[HW] Servo ready");

#if ENABLE_HX711
  Serial.println("[HW] Initialising HX711 on DOUT=D2, SCK=D3");
  scale.begin(PIN_LOADCELL_DOUT, PIN_LOADCELL_SCK);
  if (!waitForHx711Ready(HX711_READY_TIMEOUT_MS)) {
    Serial.println("[ERROR] HX711 not ready after startup retries. Check DT/SCK wiring, power, and pin mapping.");
    deviceState = STATE_ERROR;
  } else {
    Serial.println("[HW] HX711 ready");
  }
#else
  scaleReady = false;
  Serial.println("[HW] HX711 disabled; using estimated liquid weight for this test");
#endif
}

bool waitForHx711Ready(unsigned long timeoutMs) {
#if ENABLE_HX711
  unsigned long startedMs = millis();

  while (millis() - startedMs < timeoutMs) {
    if (scale.is_ready()) {
      scaleReady = true;
      return true;
    }

    delay(HX711_READY_RETRY_MS);
  }

  scaleReady = false;
  return false;
#else
  (void)timeoutMs;
  return false;
#endif
}

bool readLoadCellRawAverage(int sampleCount, long* rawValue) {
#if ENABLE_HX711
  if (!scaleReady && !waitForHx711Ready(HX711_SAMPLE_TIMEOUT_MS)) {
    return false;
  }

  if (!scale.is_ready() && !waitForHx711Ready(HX711_SAMPLE_TIMEOUT_MS)) {
    Serial.println("[WARN] HX711 not ready");
    scaleReady = false;
    return false;
  }

  loadCellRaw = scale.read_average(sampleCount);
  *rawValue = loadCellRaw;
  return true;
#else
  (void)sampleCount;
  (void)rawValue;
  return false;
#endif
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
#if ENABLE_HX711
  long rawAverage = 0;
  if (!readLoadCellRawAverage(HX711_READ_SAMPLES, &rawAverage)) {
    return false;
  }

  if (loadCellCalibrationFactor > -0.001 && loadCellCalibrationFactor < 0.001) {
    Serial.println("[CAL] Invalid calibration factor");
    return false;
  }

  liquidWeightGrams = (rawAverage - loadCellZeroRaw) / loadCellCalibrationFactor;

  if (liquidWeightGrams > -EMPTY_NOISE_GRAMS && liquidWeightGrams < EMPTY_NOISE_GRAMS) {
    liquidWeightGrams = 0.0;
  }

  if (liquidWeightGrams < 0.0) {
    liquidWeightGrams = 0.0;
  }

  return true;
#else
  loadCellRaw = 0;
  return false;
#endif
}

void printCalibrationValues() {
#if ENABLE_HX711
  String status = "Zero raw " + String(loadCellZeroRaw) + ", factor " + String(loadCellCalibrationFactor, 6);
  publishCalibrationStatus(status);
  Serial.println("[CAL] Current runtime calibration:");
  Serial.print("[CAL] LOADCELL_ZERO_RAW = ");
  Serial.println(loadCellZeroRaw);
  Serial.print("[CAL] LOADCELL_CALIBRATION_FACTOR = ");
  Serial.println(loadCellCalibrationFactor, 6);
  Serial.print("[CAL] FULL_LIQUID_GRAMS = ");
  Serial.println(FULL_LIQUID_GRAMS, 1);
  Serial.println("[CAL] Copy these values into the constants after a stable calibration.");
#else
  publishCalibrationStatus("HX711 disabled; calibration commands are unavailable");
#endif
}

void tareLoadCell() {
#if ENABLE_HX711
  long rawAverage = 0;
  if (!readLoadCellRawAverage(HX711_CALIBRATION_SAMPLES, &rawAverage)) {
    publishCalibrationStatus("Tare failed: HX711 not ready");
    return;
  }

  loadCellZeroRaw = rawAverage;
  liquidWeightGrams = 0.0;
  previousLiquidWeightGrams = 0.0;
  lastDispenseGrams = 0.0;
  updateRemainingMetrics();
  updateRefillAlert();
  updateStatusLeds();

  publishCalibrationStatus("Empty bottle set to 0 g. Zero raw " + String(loadCellZeroRaw));
  printCalibrationValues();
#else
  publishCalibrationStatus("HX711 disabled; tare skipped");
#endif
}

void calibrateFullLoadCell() {
#if ENABLE_HX711
  long rawAverage = 0;
  if (!readLoadCellRawAverage(HX711_CALIBRATION_SAMPLES, &rawAverage)) {
    publishCalibrationStatus("Full calibration failed: HX711 not ready");
    return;
  }

  long rawDelta = rawAverage - loadCellZeroRaw;
  if (rawDelta > -100 && rawDelta < 100) {
    publishCalibrationStatus("Full calibration failed: raw delta is too small");
    Serial.println("[CAL] Add the full 250 ml bottle, wait for the reading to stabilise, then send f again.");
    return;
  }

  loadCellCalibrationFactor = rawDelta / FULL_LIQUID_GRAMS;
  liquidWeightGrams = FULL_LIQUID_GRAMS;
  previousLiquidWeightGrams = FULL_LIQUID_GRAMS;
  lastDispenseGrams = 0.0;
  updateRemainingMetrics();
  updateRefillAlert();
  updateStatusLeds();

  publishCalibrationStatus("Full bottle set to 250 g. Raw " + String(rawAverage) + ", factor " + String(loadCellCalibrationFactor, 6));
  printCalibrationValues();
#else
  publishCalibrationStatus("HX711 disabled; full calibration skipped");
#endif
}

void printSerialCommandHelp() {
  Serial.println("[CMD] z = tare empty bottle as 0 g");
  Serial.println("[CMD] f = calibrate current full bottle as 250 g");
  Serial.println("[CMD] c = print current calibration values");
  Serial.println("[CMD] l = test status LEDs one by one");
  Serial.println("[CMD] o/g/y/r/w = force LED off/green/yellow/red/white");
  Serial.println("[CMD] h = print this help");
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r' || command == ' ') {
      continue;
    }

    if (command == 'z' || command == 'Z' || command == 't' || command == 'T') {
      tareLoadCell();
    } else if (command == 'f' || command == 'F') {
      calibrateFullLoadCell();
    } else if (command == 'c' || command == 'C') {
      printCalibrationValues();
    } else if (command == 'l' || command == 'L') {
      testStatusLeds();
    } else if (command == 'o' || command == 'O' || command == 'g' || command == 'G' ||
               command == 'y' || command == 'Y' || command == 'r' || command == 'R' ||
               command == 'w' || command == 'W') {
      forceStatusLedCommand(command);
    } else if (command == 'h' || command == 'H' || command == '?') {
      printSerialCommandHelp();
    } else {
      Serial.print("[CMD] Unknown command: ");
      Serial.println(command);
      printSerialCommandHelp();
    }
  }
}
#endif

void printHardwareStatus(const char* context) {
  Serial.print("[HW_STATUS] ");
  Serial.print(context);
  Serial.print(" distanceReady=");
  Serial.print(distanceSensorReady ? 1 : 0);
  Serial.print(" scaleReady=");
  Serial.print(scaleReady ? 1 : 0);
  Serial.print(" distanceMm=");
  Serial.print(distanceMm);
  Serial.print(" loadRaw=");
  Serial.print(loadCellRaw);
  Serial.print(" liquidG=");
  Serial.print(liquidWeightGrams, 1);
  Serial.print(" wifi=");
  Serial.print(WiFi.status());
  Serial.print(" blynk=");
  Serial.println(Blynk.connected() ? 1 : 0);
}

void sampleMeasurements(const char* eventName) {
#if USE_MOCK_HARDWARE
  liquidWeightGrams = (remainingPumpsEstimate * FULL_LIQUID_GRAMS) / MAX_PUMPS;
  loadCellRaw = 0;
  distanceRawMm = 0;
  distanceMm = 0;
#else
#if ENABLE_DISTANCE_SENSOR
  readDistanceMeasurement();
#else
  distanceRawMm = 0;
  distanceMm = 0;
#endif
#if ENABLE_HX711
  readWeightMeasurement();
#endif
#endif

  updateRemainingMetrics();
  updateRefillAlert();
  updateStatusLeds();
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
  Blynk.virtualWrite(PIN_CALIBRATION_STATUS, calibrationStatusMessage);
}

void setState(DeviceState nextState) {
  deviceState = nextState;
  stateStartedMs = millis();
  sendStatus();
  updateStatusLeds();
}

bool hardwareReadyForDispense() {
#if USE_MOCK_HARDWARE
  return true;
#else
#if ENABLE_HX711
#if ENABLE_DISTANCE_SENSOR
  return distanceSensorReady && scaleReady;
#else
  return scaleReady;
#endif
#else
#if ENABLE_DISTANCE_SENSOR
  return distanceSensorReady;
#else
  return true;
#endif
#endif
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
    printHardwareStatus("dispense_blocked");
    return false;
  }

  // Refill required is a warning state only. It should not block the servo
  // because the prototype still needs to dispense or be tested while low.
  return deviceState == STATE_IDLE || deviceState == STATE_REFILL_REQUIRED;
}

void startDispenseCycle() {
  if (!systemEnabled) {
    setState(STATE_DISABLED);
    return;
  }

  if (!canStartDispense()) {
    if (!hardwareReadyForDispense()) {
      printHardwareStatus("entering_error");
      setState(STATE_ERROR);
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

#if USE_MOCK_HARDWARE || !ENABLE_HX711
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
#if ENABLE_HX711
      lastDispenseGrams = previousLiquidWeightGrams - liquidWeightGrams;
      if (lastDispenseGrams < 0.0) {
        lastDispenseGrams = 0.0;
      }
#endif
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
        printHardwareStatus("state_error");
      }

      if (millis() - stateStartedMs >= ERROR_RECOVERY_MS) {
        Serial.println("[RECOVERY] Attempting system recovery");
        dispensingActive = false;
        sampleMeasurements("recovery");

        if (hardwareReadyForDispense()) {
          printHardwareStatus("recovered");
          setState(systemEnabled ? STATE_IDLE : STATE_DISABLED);
        } else {
          printHardwareStatus("recovery_failed");
          Serial.println("[RECOVERY] Hardware still not ready");
          stateStartedMs = millis();
        }
      }
      break;
  }

  if (resetAlertRequested) {
    resetAlertRequested = false;

#if USE_MOCK_HARDWARE || !ENABLE_HX711
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

BLYNK_WRITE(V13) {
  if (param.asInt() != 1) {
    return;
  }

#if !USE_MOCK_HARDWARE
  tareLoadCell();
#else
  publishCalibrationStatus("Mock hardware: tare skipped");
#endif
  Blynk.virtualWrite(PIN_CALIBRATE_ZERO, 0);
}

BLYNK_WRITE(V14) {
  if (param.asInt() != 1) {
    return;
  }

#if !USE_MOCK_HARDWARE
  calibrateFullLoadCell();
#else
  publishCalibrationStatus("Mock hardware: full calibration skipped");
#endif
  Blynk.virtualWrite(PIN_CALIBRATE_FULL, 0);
}

BLYNK_WRITE(V15) {
  if (param.asInt() != 1) {
    return;
  }

  testStatusLeds();
  Blynk.virtualWrite(PIN_LED_TEST, 0);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  setupStatusLeds();

#if !USE_MOCK_HARDWARE
  setupHardware();
  printSerialCommandHelp();
#else
  Serial.println("[MOCK] Hardware simulation enabled");
#endif

  sampleMeasurements("boot");
  connectBlynk();
  publishCalibrationStatus("Ready. Use V13 zero, V14 full 250 g, V15 LED test.");

  dispensingActive = false;
  setState(deviceState == STATE_ERROR ? STATE_ERROR : STATE_IDLE);
}

void loop() {
  runBlynkConnection();
#if !USE_MOCK_HARDWARE
  handleSerialCommands();
#endif
  updateStateMachine();
  sampleMeasurementsIfDue();

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
#if ENABLE_DISTANCE_SENSOR
  if (!readDistanceMeasurement()) {
    return false;
  }
#else
  return false;
#endif

  bool inRange = distanceMm >= MIN_HAND_DISTANCE_MM && distanceMm <= MAX_HAND_DISTANCE_MM;

  if (inRange && !handPresent) {
    handPresent = true;
    updateStatusLeds();
    Serial.print("[DISTANCE] Hand detected at ");
    Serial.print(distanceMm);
    Serial.println(" mm");
    return true;
  }

  if (!inRange) {
    if (handPresent) {
      handPresent = false;
      updateStatusLeds();
    }
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
#if ENABLE_HX711
  readWeightMeasurement();
#endif
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
