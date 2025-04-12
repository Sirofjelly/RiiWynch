#include <Arduino.h>
#include <ESP32Servo.h>
#include "Servos.h"
#include "Relays.h"
#include "StateManager.h"

const int START_RELAY_PIN = 41;

unsigned long starterRelayTime = 1000;
unsigned long rampUpDuration = 1000;
unsigned long rampDownDuration = 1000;
float rampUpExponent = 3.0;

bool startupInProgress = false;

extern bool manualMode;
extern StateManager& getGlobalStateManager();

enum StartupState {
  IDLE,
  WAIT_FOR_IDLE_REACHED,
  STARTER_ON,
  RAMP_UP,
  RAMP_DOWN_TO_TARGET,
  MANUAL_CONTROL
};

StartupState currentState = IDLE;
unsigned long stateStartTime = 0;
int rampStartAngle = 0;
int rampTargetAngle = 0;

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
        stateStartTime = millis();
        startupInProgress = true;

        if (manualMode) {
          digitalWrite(START_RELAY_PIN, LOW);
          currentState = STARTER_ON;
        } else {
          currentState = WAIT_FOR_IDLE_REACHED;
        }
      }
      break;

    case WAIT_FOR_IDLE_REACHED:
      if (millis() - stateStartTime >= 300) {
        digitalWrite(START_RELAY_PIN, LOW);
        stateStartTime = millis();
        currentState = STARTER_ON;
      }
      break;

    case STARTER_ON:
      if (millis() - stateStartTime >= starterRelayTime) {
        digitalWrite(START_RELAY_PIN, HIGH);
        stateStartTime = millis();

        if (manualMode) {
          currentState = MANUAL_CONTROL;
        } else {
          rampStartAngle = gasIdleAngle;
          rampTargetAngle = gasMaxAngle;
          currentState = RAMP_UP;
        }
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
        float shaped = pow(progress, rampUpExponent);
        int angle = rampStartAngle + shaped * (rampTargetAngle - rampStartAngle);
        gasServo.write(angle);
      }
      break;
    }

    case RAMP_DOWN_TO_TARGET: {
      float progress = float(millis() - stateStartTime) / rampDownDuration;
      if (progress >= 1.0) {
        gasServo.write(rampTargetAngle);
        currentState = MANUAL_CONTROL;  // ✅ Allows post-start speed changes
        startupInProgress = false;
      } else {
        int angle = rampStartAngle + progress * (rampTargetAngle - rampStartAngle);
        gasServo.write(angle);
      }
      break;
    }

    case MANUAL_CONTROL:
      gasServo.write(calculateTargetAngle());
      break;
  }
}
