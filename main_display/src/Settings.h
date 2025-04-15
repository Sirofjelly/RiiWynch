#pragma once
#include <Preferences.h>  // Replace EEPROM with Preferences
#include "StateManager.h"

void loadSettings();
void saveSettings();

// Declare global variables for profile management
extern int currentProfile;
extern const int totalProfiles;

// Declare the function to load settings for a specific profile
void loadSettingsForProfile(int profile);

// Declare the function to save settings for a specific profile
void saveSettingsForProfile(int profile);

extern unsigned long starterRelayTime;
extern unsigned long rampUpDuration;
extern float rampUpExponent;
extern unsigned long rampDownDuration;
extern int gasIdleAngle;
extern int gasMaxAngle;
extern int chokeAngle;
extern int brakeAngle;
extern unsigned long stopCooldownDuration;
extern bool manualMode;
extern StateManager state;
