#pragma once
#include <Preferences.h>

// Global LoRa Settings functions
void loadGlobalLoRaSettings();
void saveGlobalLoRaSettings();
void applyReceivedLoRaSettings(float freq, int power, int sf, int cr, float bw); // Apply settings from main
void applyReceivedRemoteSettings(unsigned long stopDelayMs); // Apply remote settings from main

// Global LoRa Settings (not profile-specific)
extern float loraFrequency;      // MHz (863-870 for EU)
extern int loraPower;            // dBm (2-22 for SX1262)
extern int loraSpreadingFactor;  // 7-12
extern int loraCodingRate;       // 5-8 
extern float loraBandwidth;      // kHz (7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500)

// Remote-specific settings
extern unsigned long remoteStopDelayMs; // Configurable delay before stopping motor (ms)

// Snake Game High Score
extern uint16_t snakeHighScore;
void loadSnakeHighScore();
void saveSnakeHighScore(); 