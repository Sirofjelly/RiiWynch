#pragma once

void setupStartup();
void updateStartup(bool startPressed, bool stopPressed, bool disconnectedLocalOverrideActive);

extern unsigned long starterRelayTime;
extern int stage1SpeedPercentage;
extern unsigned long stage1Duration;
extern int stage2SpeedPercentage;
extern unsigned long stage2Duration;
extern unsigned long stage3Duration;

extern bool startupInProgress;

enum StartupState {
  IDLE,
  WAIT_FOR_IDLE_REACHED,
  STARTER_ON,
  RAMP_STAGE_1,
  RAMP_STAGE_2,
  RAMP_STAGE_3,
  MANUAL_CONTROL
};

extern int calculateTargetAngle();
extern StartupState currentState;
