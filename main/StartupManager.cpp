#include <Arduino.h>
#include <ESP32Servo.h>
#include "Servos.h"
#include "Relays.h"
#include "StateManager.h"

const int START_RELAY_PIN = 41;

unsigned long starterRelayTime = 1000;
unsigned long rampUpDuration = 1000;
unsigned long rampDownDuration = 1000;

bool startupInProgress = false;

enum StartupState {
  IDLE,
  WAIT_FOR_IDLE_REACHED,
  STARTER_ON,
  RAMP_UP,
  RAMP_DOWN_TO_TARGET
};

StartupState currentState = IDLE;
unsigned long stateStartTime = 0;
int rampStartAngle = 0;
int rampTargetAngle = 0;

extern StateManager& getGlobalStateManager();

int calculateTargetAngle() {
  int p = getGlobalStateManager().getTargetPercentage();
  if (p < 5) return 0;
  if (p == 5) return gasIdleAngle;
  float step = float(p - 5) / 95.0;
  return gasIdleAngle + step * (gasMaxAngle - gasIdleAngle);
}

void setupStartup() {
  currentState = IDLE;
  startupInProgress = false;
}

void updateStartup(bool startPressed, bool stopPressed) {
  if (stopPressed || isStopRelayInCooldown()) {
    startupInProgress = false;
    currentState = IDLE;
    digitalWrite(START_RELAY_PIN, HIGH);
    gasServo.write(0);
    return;
  }

  switch (currentState) {
    case IDLE:
      if (startPressed && !startupInProgress) {
        gasServo.write(gasIdleAngle);
        startupInProgress = true;
        currentState = WAIT_FOR_IDLE_REACHED;
      }
      break;

    case WAIT_FOR_IDLE_REACHED:
      if (abs(gasServo.read() - gasIdleAngle) <= 1) {
        digitalWrite(START_RELAY_PIN, LOW);
        stateStartTime = millis();
        currentState = STARTER_ON;
      }
      break;

    case STARTER_ON:
      if (millis() - stateStartTime >= starterRelayTime) {
        digitalWrite(START_RELAY_PIN, HIGH);
        rampStartAngle = gasIdleAngle;
        rampTargetAngle = gasMaxAngle;
        stateStartTime = millis();
        currentState = RAMP_UP;
      }
      break;

    case RAMP_UP: {
      float progress = float(millis() - stateStartTime) / rampUpDuration;
      if (progress >= 1.0) {
        gasServo.write(gasMaxAngle);
        rampStartAngle = gasMaxAngle;
        rampTargetAngle = calculateTargetAngle();
        stateStartTime = millis();
        currentState = RAMP_DOWN_TO_TARGET;
      } else {
        int angle = rampStartAngle + progress * (rampTargetAngle - rampStartAngle);
        gasServo.write(angle);
      }
      break;
    }

    case RAMP_DOWN_TO_TARGET: {
      float progress = float(millis() - stateStartTime) / rampDownDuration;
      if (progress >= 1.0) {
        gasServo.write(rampTargetAngle);
        currentState = IDLE;
        startupInProgress = false;
      } else {
        int angle = rampStartAngle + progress * (rampTargetAngle - rampStartAngle);
        gasServo.write(angle);
      }
      break;
    }
  }
}