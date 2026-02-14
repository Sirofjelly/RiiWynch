#include <Arduino.h>
#include <ESP32Servo.h>
#include "Servos.h"
#include "Relays.h"
#include "StateManager.h"
#include "Settings.h"
#include "StartupManager.h"
#include "ProfileManager.h"

const int START_RELAY_PIN = 41;

bool startupInProgress = false;

extern StateManager& getGlobalStateManager();
extern ProfileManager& getGlobalProfileManager();

// Enumeration now defined in StartupManager.h

StartupState currentState = IDLE;
unsigned long stateStartTime = 0;
int rampStartAngle = 0;
int rampTargetAngle = 0;

int percentageToAngle(int p) {
  if (p < 5) return 0;
  if (p == 5) return gasIdleAngle;
  float step = float(p - 5) / 95.0;
  return gasIdleAngle + step * (gasMaxAngle - gasIdleAngle);
}

int calculateTargetAngle() {
  int p = getGlobalStateManager().getTargetPercentage();
  return percentageToAngle(p);
}

void setupStartup() {
  currentState = IDLE;
  startupInProgress = false;
}

void updateStartup(bool startPressed, bool stopPressed, bool disconnectedLocalOverrideActive) {
  if (stopPressed || isStopRelayInCooldown()) {
    startupInProgress = false;
    currentState = IDLE;
    digitalWrite(START_RELAY_PIN, HIGH);
    gasServo.write(0);
    return;
  }

  ProfileManager& profileMgr = getGlobalProfileManager();
  bool manualMode = profileMgr.isManualMode();

  switch (currentState) {
    case IDLE:
      if (manualMode && startupInProgress) {
        // If manual mode was just activated (e.g. via WebUI), force state machine reset
        startupInProgress = false;
        currentState = IDLE;
        break;
      }
      if (startPressed && !startupInProgress) {
        if (manualMode) {
          // In manual mode, set idle throttle and start cranking immediately.
          gasServo.write(gasIdleAngle);
          digitalWrite(START_RELAY_PIN, LOW);
          Serial.printf("[Startup] Manual start: gas idle=%d, starter ON for %lu ms\n",
                        gasIdleAngle, starterRelayTime);
          stateStartTime = millis();
          startupInProgress = true;
          currentState = STARTER_ON;
        } else {
          gasServo.write(gasIdleAngle);
          stateStartTime = millis();
          startupInProgress = true;
          currentState = WAIT_FOR_IDLE_REACHED;
        }
      }
      break;

    case WAIT_FOR_IDLE_REACHED:
      if (manualMode) break; // skip this state in manual mode
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
          Serial.println("[Startup] Manual start complete: starter OFF, entering MANUAL_CONTROL");
          currentState = MANUAL_CONTROL;
          startupInProgress = false;
          return;
        } else {
          // Prepare Stage 1
          rampStartAngle = gasIdleAngle;
          rampTargetAngle = percentageToAngle(stage1SpeedPercentage);
          currentState = RAMP_STAGE_1;
        }
      }
      break;

    case RAMP_STAGE_1: {
      if (manualMode) break;
      float progress = float(millis() - stateStartTime) / stage1Duration;
      if (progress >= 1.0f) {
        gasServo.write(rampTargetAngle);
        rampStartAngle = rampTargetAngle;
        rampTargetAngle = gasMaxAngle; // 100%
        stateStartTime = millis();
        currentState = RAMP_STAGE_2;
      } else {
        int angle = rampStartAngle + progress * (rampTargetAngle - rampStartAngle);
        gasServo.write(angle);
      }
      break;
    }

    case RAMP_STAGE_2: {
      if (manualMode) break;
      float progress = float(millis() - stateStartTime) / stage2Duration;
      if (progress >= 1.0f) {
        gasServo.write(gasMaxAngle);
        rampStartAngle = gasMaxAngle;
        rampTargetAngle = calculateTargetAngle();
        stateStartTime = millis();
        currentState = RAMP_STAGE_3;
      } else {
        int angle = rampStartAngle + progress * (rampTargetAngle - rampStartAngle);
        gasServo.write(angle);
      }
      break;
    }

    case RAMP_STAGE_3: {
      if (manualMode) break;
      float progress = float(millis() - stateStartTime) / stage3Duration;

      if (disconnectedLocalOverrideActive) {
        int requestedTargetAngle = calculateTargetAngle();
        if (requestedTargetAngle != rampTargetAngle) {
          int currentAngle = (progress >= 1.0f)
            ? rampTargetAngle
            : (rampStartAngle + progress * (rampTargetAngle - rampStartAngle));
          Serial.printf("[Startup] Stage 3 retarget while remote disconnected: %d -> %d\n",
                        rampTargetAngle, requestedTargetAngle);
          rampStartAngle = currentAngle;
          rampTargetAngle = requestedTargetAngle;
          stateStartTime = millis();
          progress = 0.0f;
        }
      }

      if (progress >= 1.0f) {
        gasServo.write(rampTargetAngle);
        currentState = MANUAL_CONTROL;
        startupInProgress = false;
      } else {
        int angle = rampStartAngle + progress * (rampTargetAngle - rampStartAngle);
        gasServo.write(angle);
      }
      break;
    }

    case MANUAL_CONTROL:
      if (stopPressed) {
        currentState = IDLE;
      }
      // In manual mode, servo is only moved by percentage changes elsewhere
      break;
  }
}
