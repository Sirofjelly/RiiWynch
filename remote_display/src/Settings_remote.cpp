#include <Preferences.h>
#include "Settings_remote.h"

// Create a preferences instance
Preferences preferences;

// Global LoRa Settings (not profile-specific) - default values
float loraFrequency = 868.0;      // MHz (EU band)
int loraPower = 14;               // dBm (safe default)
int loraSpreadingFactor = 8;      // good range/speed balance
int loraCodingRate = 5;           // error correction
float loraBandwidth = 125.0;      // kHz (standard)

// Remote-specific settings - default values
unsigned long remoteStopDelayMs = 1000; // 1 second default delay
uint16_t snakeHighScore = 0;

void loadSnakeHighScore() {
    if (!preferences.begin("riiwynch_remote", true)) return;
    snakeHighScore = preferences.getUShort("snake_hi", 0);
    preferences.end();
    Serial.printf("🎮 [Remote] Loaded Snake high score: %d\n", snakeHighScore);
}

void saveSnakeHighScore() {
    if (!preferences.begin("riiwynch_remote", false)) return;
    preferences.putUShort("snake_hi", snakeHighScore);
    preferences.end();
    Serial.printf("💾 [Remote] Saved Snake high score: %d\n", snakeHighScore);
}

// Global Settings Functions (LoRa settings for remote)
void loadGlobalLoRaSettings() {
    Serial.println("📡 [Remote] Loading global LoRa settings from preferences...");
    
    // Begin preferences with "riiwynch_remote" namespace, read-write mode
    if (!preferences.begin("riiwynch_remote", false)) {
        Serial.println("⚠️ [Remote] Failed to initialize Preferences for global settings");
        return;
    }
    
    // Check if global settings exist
    bool globalExists = preferences.getBool("global_init", false);

    if (!globalExists) {
        Serial.println("⚠️ [Remote] Global settings not initialized — using defaults.");
        
        // Use current default values (already set above)
        // Save these defaults
        saveGlobalLoRaSettings();
        preferences.end();
        return;
    }

    // Cache old values for comparison
    float oldFreq = loraFrequency;
    int oldPower = loraPower;
    
    // Load global settings
    loraFrequency = preferences.getFloat("global_freq", 868.0);
    loraPower = preferences.getInt("global_power", 14);
    loraSpreadingFactor = preferences.getInt("global_sf", 8);
    loraCodingRate = preferences.getInt("global_cr", 5);
    loraBandwidth = preferences.getFloat("global_bw", 125.0);
    
    // Load remote-specific settings
    remoteStopDelayMs = preferences.getULong("stop_delay_ms", 1000);
    
    Serial.printf("✅ [Remote] Loaded global LoRa settings from preferences\n");
    Serial.printf("  Frequency: %.1f MHz (was %.1f)\n", loraFrequency, oldFreq);
    Serial.printf("  Power: %d dBm (was %d)\n", loraPower, oldPower);
    Serial.printf("  SF: %d, CR: %d, BW: %.1f kHz\n", loraSpreadingFactor, loraCodingRate, loraBandwidth);
    Serial.printf("  Stop Delay: %lu ms\n", remoteStopDelayMs);
    
    // Close preferences
    preferences.end();
}

void saveGlobalLoRaSettings() {
    Serial.println("💾 [Remote] Saving global LoRa settings...");
    Serial.printf("  Frequency: %.1f MHz\n", loraFrequency);
    Serial.printf("  Power: %d dBm\n", loraPower);
    Serial.printf("  SF: %d, CR: %d, BW: %.1f kHz\n", loraSpreadingFactor, loraCodingRate, loraBandwidth);
    
    // Begin preferences with "riiwynch_remote" namespace, read-write mode
    if (!preferences.begin("riiwynch_remote", false)) {
        Serial.println("⚠️ [Remote] Failed to initialize Preferences for global settings");
        return;
    }
    
    // Mark global settings as initialized
    bool ok = preferences.putBool("global_init", true);

    // Save all global settings and check each result
    ok &= preferences.putFloat("global_freq", loraFrequency);
    ok &= preferences.putInt("global_power", loraPower);
    ok &= preferences.putInt("global_sf", loraSpreadingFactor);
    ok &= preferences.putInt("global_cr", loraCodingRate);
    ok &= preferences.putFloat("global_bw", loraBandwidth);
    
    // Save remote-specific settings
    ok &= preferences.putULong("stop_delay_ms", remoteStopDelayMs);

    preferences.end();

    if (ok) {
        Serial.println("✅ [Remote] Global settings successfully saved to preferences");
    } else {
        Serial.println("⚠️ [Remote] Failed to save one or more global settings to preferences.");
    }
}

void applyReceivedLoRaSettings(float freq, int power, int sf, int cr, float bw) {
    Serial.printf("📡 [Remote] Received LoRa settings from main: freq=%.1f MHz, power=%d dBm, SF=%d, CR=%d, BW=%.1f kHz\n", 
                  freq, power, sf, cr, bw);
    
    // Update global variables
    loraFrequency = freq;
    loraPower = power;
    loraSpreadingFactor = sf;
    loraCodingRate = cr;
    loraBandwidth = bw;
    
    // Save to EEPROM
    saveGlobalLoRaSettings();
    
    Serial.println("✅ [Remote] LoRa settings applied and saved");
}

void applyReceivedRemoteSettings(unsigned long stopDelayMs) {
    Serial.printf("📡 [Remote] Received remote settings from main: stopDelayMs=%lu ms\n", stopDelayMs);
    
    // Update global variable
    remoteStopDelayMs = stopDelayMs;
    
    // Save to preferences
    saveGlobalLoRaSettings(); // This now also saves remote settings
    
    Serial.println("✅ [Remote] Remote settings applied and saved");
} 