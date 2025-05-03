#pragma once

void setupStartup();
void updateStartup(bool startPressed, bool stopPressed);

extern unsigned long starterRelayTime;
extern unsigned long rampUpDuration;
extern unsigned long rampDownDuration;
extern float rampUpExponent;
extern bool manualMode;
extern bool startupInProgress;

enum StartupState {
  IDLE,
  WAIT_FOR_IDLE_REACHED,
  STARTER_ON,
  RAMP_UP,
  RAMP_DOWN_TO_TARGET,
  MANUAL_CONTROL
};

extern int calculateTargetAngle();
extern StartupState currentState;
