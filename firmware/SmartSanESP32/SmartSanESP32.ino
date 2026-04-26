/*
  SmartSan ESP32 firmware skeleton.

  This sketch is designed to run before the final hardware arrives. By default it
  uses simulated IR, servo, and HX711 behaviour so the Blynk dashboard and state
  machine can be tested early. Set USE_MOCK_HARDWARE to 0 when the real sensors
  are ready and replace the hardware adapter functions near the bottom.
*/

#define BLYNK_TEMPLATE_ID "REPLACE_WITH_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "SmartSan Prototype"
#define BLYNK_AUTH_TOKEN "REPLACE_WITH_DEVICE_AUTH_TOKEN"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define USE_MOCK_HARDWARE 1

const char WIFI_SSID[] = "REPLACE_WITH_WIFI_NAME";
const char WIFI_PASS[] = "REPLACE_WITH_WIFI_PASSWORD";

const int PIN_USAGE_COUNT = V0;
const int PIN_CURRENT_WEIGHT = V1;
const int PIN_REMAINING_PERCENT = V2;
const int PIN_REFILL_ALERT = V3;
const int PIN_DEVICE_STATE = V4;
const int PIN_LAST_DISPENSE_AT = V5;
const int PIN_DEVICE_ONLINE = V6;
const int PIN_MANUAL_DISPENSE = V10;
const int PIN_RESET_ALERT = V11;
const int PIN_SYSTEM_ENABLED = V12;

const float FULL_WEIGHT_G = 520.0;
const float EMPTY_WEIGHT_G = 120.0;
const float REFILL_THRESHOLD_G = 250.0;
const unsigned long DISPENSE_LOCKOUT_MS = 2000;
const unsigned long STABILISE_DELAY_MS = 1200;
const unsigned long STATUS_INTERVAL_MS = 5000;

enum DeviceState {
  IDLE,
  HAND_DETECTED,
  DISPENSING,
  WAIT_STABILISE,
  READ_WEIGHT,
  UPDATE_STATUS,
  REFILL_REQUIRED,
  DISABLED,
  ERROR_STATE
};

DeviceState deviceState = IDLE;

int usageCount = 0;
float currentWeight = FULL_WEIGHT_G;
bool refillAlert = false;
bool systemEnabled = true;
bool manualDispenseRequested = false;
bool resetAlertRequested = false;
unsigned long lastDispenseMs = 0;
unsigned long stateStartedMs = 0;
unsigned long lastStatusMs = 0;

String stateToString(DeviceState state) {
  switch (state) {
    case IDLE:
      return "IDLE";
    case HAND_DETECTED:
      return "HAND_DETECTED";
    case DISPENSING:
      return "DISPENSING";
    case WAIT_STABILISE:
      return "WAIT_STABILISE";
    case READ_WEIGHT:
      return "READ_WEIGHT";
    case UPDATE_STATUS:
      return "UPDATE_STATUS";
    case REFILL_REQUIRED:
      return "REFILL_REQUIRED";
    case DISABLED:
      return "DISABLED";
    default:
      return "ERROR";
  }
}

int calculateRemainingPercent(float weight) {
  float usableRange = FULL_WEIGHT_G - EMPTY_WEIGHT_G;
  float percent = ((weight - EMPTY_WEIGHT_G) / usableRange) * 100.0;

  if (percent < 0) {
    return 0;
  }

  if (percent > 100) {
    return 100;
  }

  return round(percent);
}

void setState(DeviceState nextState) {
  deviceState = nextState;
  stateStartedMs = millis();
  sendStatus();
}

void sendStatus() {
  Blynk.virtualWrite(PIN_USAGE_COUNT, usageCount);
  Blynk.virtualWrite(PIN_CURRENT_WEIGHT, currentWeight);
  Blynk.virtualWrite(PIN_REMAINING_PERCENT, calculateRemainingPercent(currentWeight));
  Blynk.virtualWrite(PIN_REFILL_ALERT, refillAlert ? 1 : 0);
  Blynk.virtualWrite(PIN_DEVICE_STATE, stateToString(deviceState));
  Blynk.virtualWrite(PIN_LAST_DISPENSE_AT, lastDispenseMs == 0 ? "--" : String(lastDispenseMs / 1000) + "s");
  Blynk.virtualWrite(PIN_DEVICE_ONLINE, Blynk.connected() ? 1 : 0);
}

bool canStartDispense() {
  if (!systemEnabled) {
    setState(DISABLED);
    return false;
  }

  if (millis() - lastDispenseMs < DISPENSE_LOCKOUT_MS) {
    return false;
  }

  return deviceState == IDLE || deviceState == REFILL_REQUIRED;
}

void startDispenseCycle() {
  if (!canStartDispense()) {
    return;
  }

  setState(HAND_DETECTED);
}

void updateStateMachine() {
  switch (deviceState) {
    case IDLE:
    case REFILL_REQUIRED:
      if (manualDispenseRequested || readHandDetected()) {
        manualDispenseRequested = false;
        startDispenseCycle();
      }
      break;

    case HAND_DETECTED:
      performDispense();
      setState(DISPENSING);
      break;

    case DISPENSING:
      usageCount += 1;
      lastDispenseMs = millis();
      setState(WAIT_STABILISE);
      break;

    case WAIT_STABILISE:
      if (millis() - stateStartedMs >= STABILISE_DELAY_MS) {
        setState(READ_WEIGHT);
      }
      break;

    case READ_WEIGHT:
      currentWeight = readStableWeight();
      refillAlert = currentWeight <= REFILL_THRESHOLD_G;
      setState(UPDATE_STATUS);
      break;

    case UPDATE_STATUS:
      setState(refillAlert ? REFILL_REQUIRED : IDLE);
      break;

    case DISABLED:
      if (systemEnabled) {
        setState(refillAlert ? REFILL_REQUIRED : IDLE);
      }
      break;

    case ERROR_STATE:
      break;
  }

  if (resetAlertRequested) {
    resetAlertRequested = false;
    currentWeight = FULL_WEIGHT_G;
    refillAlert = false;
    setState(systemEnabled ? IDLE : DISABLED);
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
  setState(systemEnabled ? IDLE : DISABLED);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
  setState(IDLE);
}

void loop() {
  Blynk.run();
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
  return command == 'd' || command == 'D';
#else
  // TODO: Replace with real IR sensor read, including threshold/debounce logic.
  return false;
#endif
}

void performDispense() {
#if USE_MOCK_HARDWARE
  Serial.println("Mock dispense: servo press and return");
#else
  // TODO: Replace with servo write angles and timing once the mechanism is built.
#endif
}

float readStableWeight() {
#if USE_MOCK_HARDWARE
  currentWeight -= 18.0;

  if (currentWeight < EMPTY_WEIGHT_G) {
    currentWeight = EMPTY_WEIGHT_G;
  }

  return currentWeight;
#else
  // TODO: Replace with HX711 averaging after tare/calibration.
  return currentWeight;
#endif
}
