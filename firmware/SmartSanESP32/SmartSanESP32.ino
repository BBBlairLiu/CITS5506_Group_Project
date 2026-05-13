/*
  SmartSan ESP32 firmware skeleton.

  This sketch uses simulated IR and servo behaviour so the Blynk dashboard and state
  machine can be tested early. Weight sensing (HX711) is planned but currently
  replaced with pump-count logic for the prototype.
  Set USE_MOCK_HARDWARE to 0 when the real sensors
  are ready and replace the hardware adapter functions near the bottom.
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

// IR sensor configuration - adjust as needed for the actual sensor and placement
#define PIN_IR_SENSOR     2
#define IR_ACTIVE_LOW     true
#define IR_DEBOUNCE_MS    80

// Servo configuration - adjust pin and angles as needed for the actual mechanism
#define PIN_SERVO         5

#define USE_MOCK_HARDWARE 1 // Set to 0 to use real hardware when ready

// Hardware libraries (servo only used when not in mock mode)
#if !USE_MOCK_HARDWARE // Only include servo library if not using mock hardware, to avoid unnecessary dependencies during early testing
#include <ESP32Servo.h>
#endif

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

//ir sensor variables for debounce logic and stable state detection
static bool _irLastState = false;
static bool _irStableState = false;
static unsigned long _irLastChangeMs = 0;
static bool _dispensingActive = false;  // prevents overlapping dispense cycles
static bool _irEventConsumed = true;

// Servo timing and angles - adjust as needed for the actual mechanism
const int SERVO_HOME_DEG  = 0;
const int SERVO_PRESS_DEG = 60;
const int SERVO_PRESS_MS  = 400;
const int SERVO_RETURN_MS = 300;
const int SERVO_SETTLE_MS = 100;

// Blynk virtual pin definitions
const int PIN_USAGE_COUNT = V0;
const int PIN_REMAINING_PERCENT = V2;
const int PIN_REFILL_ALERT = V3;
const int PIN_DEVICE_STATE = V4;
const int PIN_LAST_DISPENSE_AT = V5;
const int PIN_DEVICE_ONLINE = V6;
const int PIN_REMAINING_PUMPS = V7; // NEW: raw remaining pumps for debugging, replaced current weight reading pin
const int PIN_MANUAL_DISPENSE = V10;
const int PIN_RESET_ALERT = V11;
const int PIN_SYSTEM_ENABLED = V12;

// Device and state machine variables
const int MAX_PUMPS = 25; // TODO: calibrate by counting full bottle dispenses
const unsigned long DISPENSE_LOCKOUT_MS = 2000;
const unsigned long STABILISE_DELAY_MS = 1200;
const unsigned long STATUS_INTERVAL_MS = 5000;
const unsigned long STABILISE_TIMEOUT_MS = 5000; // watchdog timeout for dispense cycle
const unsigned long ERROR_RECOVERY_MS = 8000; // Time to wait in error state before allowing reset, can be adjusted based on expected recovery time or user intervention needs

unsigned long lastErrorLogMs = 0; // For rate-limiting error logs to avoid spamming during error conditions

// State machine states
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

// Global variables
DeviceState deviceState = STATE_IDLE;

#if !USE_MOCK_HARDWARE
Servo dispenserServo; // Servo object for controlling the dispenser mechanism
#endif
int usageCount = 0;
int sessionCount = 0;
int remainingPumps = MAX_PUMPS;
bool refillAlert = false;
bool systemEnabled = true;
bool manualDispenseRequested = false;
bool resetAlertRequested = false;
unsigned long lastDispenseMs = 0;
unsigned long stateStartedMs = 0;
unsigned long lastStatusMs = 0;

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
      Serial.println("[WARN] Unknown DeviceState encountered");
      return "UNKNOWN_STATE";
  }
}

void setState(DeviceState nextState) {
  deviceState = nextState;
  stateStartedMs = millis();
  sendStatus();
}

void sendStatus() { //updating logic with pump count instead of weight
  int remainingPercent = (remainingPumps * 100) / MAX_PUMPS;
  remainingPercent = constrain(remainingPercent, 0, 100);

  Blynk.virtualWrite(PIN_USAGE_COUNT, usageCount);
  Blynk.virtualWrite(PIN_REMAINING_PERCENT, remainingPercent);
  Blynk.virtualWrite(PIN_REFILL_ALERT, refillAlert ? 1 : 0);
  Blynk.virtualWrite(PIN_DEVICE_STATE, stateToString(deviceState));
  Blynk.virtualWrite(PIN_LAST_DISPENSE_AT, lastDispenseMs == 0 ? "--" : String(lastDispenseMs / 1000) + "s");
  Blynk.virtualWrite(PIN_DEVICE_ONLINE, Blynk.connected() ? 1 : 0);

  // NEW: raw remaining pumps
  Blynk.virtualWrite(PIN_REMAINING_PUMPS, remainingPumps); //cleaner version
}

bool canStartDispense() { 
  // hard single-dispense lock
  if (_dispensingActive) {
    Serial.println("[GUARD] Dispense already active");
    return false;
  }

  if (!systemEnabled) {
    return false;
  }

  if (millis() - lastDispenseMs < DISPENSE_LOCKOUT_MS) {
    return false;
  }

  if (remainingPumps <= 0) {
    return false;
  }

  return deviceState == STATE_IDLE || deviceState == STATE_REFILL_REQUIRED;
}

void startDispenseCycle() { // updated to check if dispense can start, and set active flag to prevent multiple triggers during dispensing

  if (!systemEnabled) {
    setState(STATE_DISABLED);
    return;
  }

  if (!canStartDispense()) {

    if (remainingPumps <= 0) {
      Serial.println("[BLOCK] Bottle empty");
      setState(STATE_REFILL_REQUIRED);
    }

    return;
  }

  _dispensingActive = true;

  setState(STATE_HAND_DETECTED);
}

void updateStateMachine() {
  switch (deviceState) {
    case STATE_IDLE:
    case STATE_REFILL_REQUIRED:
      if (manualDispenseRequested) {
        manualDispenseRequested = false;
        startDispenseCycle();
      }
      else if (readHandDetected()) {
        startDispenseCycle();
      }
      break;

    case STATE_HAND_DETECTED:
      performDispense();
      setState(STATE_DISPENSING);
      break;

    case STATE_DISPENSING: // update usage and remaining pump count
      if (sessionCount < MAX_PUMPS) {
        usageCount += 1;
        sessionCount += 1;
      }

      remainingPumps = MAX_PUMPS - sessionCount;

      lastDispenseMs = millis();

      // release dispense lock
      _dispensingActive = false;

      Serial.print("[COUNT] Total: ");
      Serial.print(usageCount);
      Serial.print(" | Session: ");
      Serial.print(sessionCount);
      Serial.print(" | Remaining: ");
      Serial.println(remainingPumps);

      setState(STATE_WAIT_STABILISE);
      break;

    case STATE_WAIT_STABILISE: // allow dispense cycle to fully complete before next state

      if (millis() - stateStartedMs >= STABILISE_DELAY_MS) {
        setState(STATE_CHECK_REFILL);
        break;
      }

      // watchdog protection
      if (millis() - stateStartedMs >= STABILISE_TIMEOUT_MS) {
        Serial.println("[ERROR] WAIT_STABILISE timeout");
        setState(STATE_ERROR);
      }

      break;

    case STATE_CHECK_REFILL: 
      refillAlert = (remainingPumps <= 0);
      setState(STATE_UPDATE_STATUS);
      break;

    case STATE_UPDATE_STATUS:
      setState(refillAlert ? STATE_REFILL_REQUIRED : STATE_IDLE);
      break;

    case STATE_DISABLED: // Ensure system is fully inactive and reset session count, but keep total usage for stats
      _dispensingActive = false;

      if (systemEnabled) {
        setState(refillAlert ? STATE_REFILL_REQUIRED : STATE_IDLE);
      }

      break;

    case STATE_ERROR: // In error state, block all actions and require manual reset, but allow status updates to show error condition

      if (millis() - lastErrorLogMs >= 2000) {
        lastErrorLogMs = millis();
        Serial.println("[ERROR] System in STATE_ERROR");
      }

      if (millis() - stateStartedMs >= ERROR_RECOVERY_MS) {

        Serial.println("[RECOVERY] Recovering system");

        _dispensingActive = false;

        setState(systemEnabled ? STATE_IDLE : STATE_DISABLED);
      }

      break;
  }

  if (resetAlertRequested) { // Reset session count and refill alert, but keep total usage count.
    resetAlertRequested = false;

    sessionCount = 0;
    remainingPumps = MAX_PUMPS;
    refillAlert = false;

    _dispensingActive = false;  // FIX: unlock dispensing state

    Serial.println("[RESET] Refill confirmed");

    setState(systemEnabled ? STATE_IDLE : STATE_DISABLED);
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
  setState(systemEnabled ? STATE_IDLE : STATE_DISABLED);
}

void setup() { 
  Serial.begin(115200);
  delay(100);

  pinMode(PIN_IR_SENSOR, INPUT); // Set IR sensor pin as input

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS); // Connect to WiFi and Blynk

  #if !USE_MOCK_HARDWARE
    dispenserServo.attach(PIN_SERVO); // Attach servo to pin  
    dispenserServo.write(SERVO_HOME_DEG); // Move servo to home position
    delay(500);
  #endif

  _dispensingActive = false;
  setState(STATE_IDLE);
}

void loop() {
  Blynk.run();
  updateStateMachine();

  if (millis() - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = millis();
    sendStatus();
  }
}

bool readHandDetected() { //updated logic to use IR sensor with debounce and stable state detection, replacing the old mock logic of reading from serial input
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
    bool rawDetected = IR_ACTIVE_LOW
      ? (digitalRead(PIN_IR_SENSOR) == LOW)
      : (digitalRead(PIN_IR_SENSOR) == HIGH);

    unsigned long now = millis();

    if (rawDetected != _irLastState) {
      _irLastChangeMs = now;
      _irLastState = rawDetected;
    }

    if ((now - _irLastChangeMs) >= IR_DEBOUNCE_MS) {
      _irStableState = _irLastState;
    }

    if (_irStableState && _irEventConsumed) {
      _irEventConsumed = false;
      return false;
    }

    if (_irStableState && !_irEventConsumed) {
      _irEventConsumed = true;
      Serial.println("[IR] Hand detected");
      return true;
    }

    if (!_irStableState) {
      _irEventConsumed = true;
    }

    return false;
  #endif
}

//updated to use servo for dispensing, replacing old mock logic of reading from serial input
void performDispense() {
#if USE_MOCK_HARDWARE
  Serial.println("[MOCK] Dispense start");
  delay(200);
  Serial.println("[MOCK] Dispense end");

#else
  Serial.println("[SERVO] Press started");

  delay(SERVO_SETTLE_MS);

  dispenserServo.write(SERVO_PRESS_DEG);
  delay(SERVO_PRESS_MS);

  dispenserServo.write(SERVO_HOME_DEG);
  delay(SERVO_RETURN_MS);

  Serial.println("[SERVO] Cycle complete");
#endif
}
