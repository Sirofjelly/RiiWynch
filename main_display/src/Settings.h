#pragma once
#include <Preferences.h>  // Replace EEPROM with Preferences
#include "StateManager.h"

void loadSettings();
void saveSettings();

// Declare global variables for profile management
// 0: SURF, 1: SKIM, 2: SMOOTH, 3: MANUAL
extern int currentProfile;
extern const int totalProfiles;

// Declare the function to load settings for a specific profile
void loadSettingsForProfile(int profile);

// Declare the function to save settings for a specific profile
void saveSettingsForProfile(int profile);

// Declare global settings functions (non-profile specific)
void loadGlobalSettings();
void saveGlobalSettings();

extern unsigned long starterRelayTime;
extern int stage1SpeedPercentage;  // Target speed for stage 1 (5-100)
extern unsigned long stage1Duration; // ms – ramp to stage1SpeedPercentage
extern unsigned long stage2Duration; // ms – ramp from stage1SpeedPercentage to 100%
extern unsigned long stage3Duration; // ms – ramp from 100% down to target percentage
extern int gasIdleAngle;
extern int gasMaxAngle;
extern int chokeAngle;
extern int brakeAngle;
extern unsigned long stopCooldownDuration;
extern bool manualMode;
extern StateManager state;

// Global LoRa Settings (not profile-specific)
extern float loraFrequency;      // MHz (863-870 for EU)
extern int loraPower;            // dBm (2-22 for SX1262)
extern int loraSpreadingFactor;  // 7-12
extern int loraCodingRate;       // 5-8 
extern float loraBandwidth;      // kHz (7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500)
