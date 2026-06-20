
#include "StateManager.h"
extern StateManager state;
extern bool manualMode;

unsigned long manualModeHoldStart = 0;
const unsigned long manualModeHoldDuration = 2000; // 2 seconds

#include <Arduino.h>

const int START_BUTTON_PIN = 2;
const int STOP_BUTTON_PIN = 4;
const int CHOKE_BUTTON_PIN = 5;
const int BRAKE_BUTTON_PIN = 6;

void setupButtons() {
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CHOKE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BRAKE_BUTTON_PIN, INPUT_PULLUP);
}

bool isStartPressed() {
  return digitalRead(START_BUTTON_PIN) == LOW;
}

bool isStopPressed() {
  // Fail-safe stop input: a normally-closed stop switch should hold the pin LOW.
  // Pressed/open/broken-wire reads HIGH and forces STOP.
  return digitalRead(STOP_BUTTON_PIN) == HIGH;
}

bool isChokePressed() {
  return digitalRead(CHOKE_BUTTON_PIN) == LOW;
}

bool isBrakePressed() {
  return digitalRead(BRAKE_BUTTON_PIN) == LOW;
}