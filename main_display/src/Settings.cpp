#include <Preferences.h>
#include "Servos.h"
#include "StartupManager.h"
#include "Relays.h"
#include "StateManager.h"
#include "Settings.h"
#include "ProfileManager.h"

// Forward declarations of functions
void loadSettings();
void saveSettings();
void loadSettingsForProfile(int profileIndex);
void saveSettingsForProfile(int profileIndex);

// Forward declaration for global ProfileManager accessor
extern ProfileManager& getGlobalProfileManager();

// Create a preferences instance
Preferences preferences;

unsigned long starterRelayTime = 1500;
int stage1SpeedPercentage = 30;           // % of max speed (5-100)
unsigned long stage1Duration = 1000;      // ms
int stage2SpeedPercentage = 100;           // % of max speed (5-100) - stage 2 target speed
unsigned long stage2Duration = 2000;      // ms
unsigned long stage3Duration = 1500;      // ms
int gasIdleAngle = 15;
int gasMaxAngle = 65;
int chokeAngle = 55;
int brakeAngle = 55;
unsigned long stopCooldownDuration = 3000;
bool manualMode = false;

// Define global variables for profile management
const int totalProfiles = 4; // Updated to 4 profiles

// Ensure `currentProfile` is defined only here
int currentProfile = 0;

// Global LoRa Settings (not profile-specific) - default values
float loraFrequency = 868.0;      // MHz (EU band)
int loraPower = 14;               // dBm (safe default)
int loraSpreadingFactor = 8;      // good range/speed balance
int loraCodingRate = 5;           // error correction
float loraBandwidth = 125.0;      // kHz (standard)

// Remote-specific settings - default values
unsigned long remoteStopDelayMs = 1000; // 1 second default delay

// Define persistent statistics
unsigned long totalStarts = 0;
unsigned long totalRuntimeSeconds = 0;

void loadSettings() {
  // Load settings for the current profile. loadSettingsForProfile() owns the
  // Preferences session; nesting begin()/end() on the same global Preferences
  // object can corrupt subsequent reads/writes.
  loadSettingsForProfile(currentProfile);
}

void saveSettings() {
  // Use the profile-specific save function with the current profile
  saveSettingsForProfile(currentProfile);
  
  Serial.println("💾 Settings saved to the current profile.");
}

// Implement the function to load settings for a specific profile
void loadSettingsForProfile(int profileIndex) {
    Serial.printf("📂 Loading profile %d from preferences...\n", profileIndex + 1);
    
    // Begin preferences with "riiwynch" namespace, read-write mode
    if (!preferences.begin("riiwynch", false)) {
        Serial.println("⚠️ Failed to initialize Preferences");
        return;
    }
    
    // Check if this profile exists (using a flag)
    String profileKey = "prof_init_" + String(profileIndex);
    bool profileExists = preferences.getBool(profileKey.c_str(), false);

    if (!profileExists) {
        Serial.println("⚠️ Profile not initialized — initializing with defaults.");
        
        // Set default values for this profile
        starterRelayTime     = 1500;
        stage1SpeedPercentage = 30;
        stage1Duration        = 1000;
        stage2SpeedPercentage = 60;
        stage2Duration        = 2000;
        stage3Duration        = 1500;
        gasIdleAngle         = 15;
        gasMaxAngle          = 65;
        chokeAngle           = 55;
        brakeAngle           = 55;
        stopCooldownDuration = 3000;
        manualMode           = (profileIndex == 3);
        
        // Save these defaults to this profile's section. Close this read
        // session first; saveSettingsForProfile() opens its own write session.
        preferences.end();
        saveSettingsForProfile(profileIndex);
        return;
    }

    // Cache the old values for comparison
    unsigned long oldStarterTime = starterRelayTime;
    int oldGasIdle = gasIdleAngle;
    
    // Create keys for this profile
    String baseKey = "p" + String(profileIndex) + "_";
    
    // Load settings from this profile's section
    starterRelayTime     = preferences.getULong(String(baseKey + "starter").c_str(), 1500);
    stage1SpeedPercentage = preferences.getInt(String(baseKey + "s1spd").c_str(), 30);
    stage1Duration        = preferences.getULong(String(baseKey + "s1dur").c_str(), 1000);
    stage2SpeedPercentage = preferences.getInt(String(baseKey + "s2spd").c_str(), 100);
    stage2Duration        = preferences.getULong(String(baseKey + "s2dur").c_str(), 2000);
    stage3Duration        = preferences.getULong(String(baseKey + "s3dur").c_str(), 1500);
    gasIdleAngle         = preferences.getInt(String(baseKey + "gasidle").c_str(), 15);
    gasMaxAngle          = preferences.getInt(String(baseKey + "gasmax").c_str(), 65);
    chokeAngle           = preferences.getInt(String(baseKey + "choke").c_str(), 55);
    brakeAngle           = preferences.getInt(String(baseKey + "brake").c_str(), 55);
    stopCooldownDuration = preferences.getULong(String(baseKey + "cooldown").c_str(), 3000);
    // Manual mode is selected by profile index, not by persisted profile payload.
    manualMode           = (profileIndex == 3);
    
    Serial.printf("✅ Loaded profile %d from preferences\n", profileIndex + 1);
    Serial.printf("  Starter Relay Time: %lu ms (was %lu)\n", starterRelayTime, oldStarterTime);
    Serial.printf("  Gas Idle Angle: %d° (was %d)\n", gasIdleAngle, oldGasIdle);
    
    // Close preferences
    preferences.end();
}

void saveSettingsForProfile(int profileIndex) {
    Serial.printf("💾 Saving profile %d with values:\n", profileIndex + 1);
    Serial.printf("  Starter Relay Time: %lu ms\n", starterRelayTime);
    Serial.printf("  Gas Idle Angle: %d°\n", gasIdleAngle);
    
    // Begin preferences with "riiwynch" namespace, read-write mode
    if (!preferences.begin("riiwynch", false)) {
        Serial.println("⚠️ Failed to initialize Preferences");
        return;
    }
    
    // Create keys for this profile
    String baseKey = "p" + String(profileIndex) + "_";
    
    // Mark this profile as initialized
    String profileKey = "prof_init_" + String(profileIndex);
    bool ok = preferences.putBool(profileKey.c_str(), true);

    // Save all settings and check each result
    ok &= preferences.putULong(String(baseKey + "starter").c_str(), starterRelayTime);
    ok &= preferences.putInt(String(baseKey + "s1spd").c_str(), stage1SpeedPercentage);
    ok &= preferences.putULong(String(baseKey + "s1dur").c_str(), stage1Duration);
    ok &= preferences.putInt(String(baseKey + "s2spd").c_str(), stage2SpeedPercentage);
    ok &= preferences.putULong(String(baseKey + "s2dur").c_str(), stage2Duration);
    ok &= preferences.putULong(String(baseKey + "s3dur").c_str(), stage3Duration);
    ok &= preferences.putInt(String(baseKey + "gasidle").c_str(), gasIdleAngle);
    ok &= preferences.putInt(String(baseKey + "gasmax").c_str(), gasMaxAngle);
    ok &= preferences.putInt(String(baseKey + "choke").c_str(), chokeAngle);
    ok &= preferences.putInt(String(baseKey + "brake").c_str(), brakeAngle);
    ok &= preferences.putULong(String(baseKey + "cooldown").c_str(), stopCooldownDuration);
    // Persist a deterministic value for backward compatibility with existing keys.
    ok &= preferences.putBool(String(baseKey + "manual").c_str(), profileIndex == 3);

    preferences.end();

    if (ok) {
        Serial.println("✅ Settings successfully saved to preferences");
    } else {
        Serial.println("⚠️ Failed to save one or more settings to preferences. Possible NVS corruption or out of space.");
    }
}

// Global Settings Functions (LoRa settings that apply to all profiles)
void loadGlobalSettings() {
    Serial.println("📡 Loading global LoRa settings from preferences...");
    
    // Begin preferences with "riiwynch" namespace, read-write mode
    if (!preferences.begin("riiwynch", false)) {
        Serial.println("⚠️ Failed to initialize Preferences for global settings");
        return;
    }
    
    // Check if global settings exist
    bool globalExists = preferences.getBool("global_init", false);

    if (!globalExists) {
        Serial.println("⚠️ Global settings not initialized — initializing with defaults.");
        
        // Use current default values (already set above). Close this read
        // session first; saveGlobalSettings() opens its own write session.
        preferences.end();
        saveGlobalSettings();
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
    
    Serial.printf("✅ Loaded global LoRa settings from preferences\n");
    Serial.printf("  Frequency: %.1f MHz (was %.1f)\n", loraFrequency, oldFreq);
    Serial.printf("  Power: %d dBm (was %d)\n", loraPower, oldPower);
    Serial.printf("  SF: %d, CR: %d, BW: %.1f kHz\n", loraSpreadingFactor, loraCodingRate, loraBandwidth);
    Serial.printf("  Remote Stop Delay: %lu ms\n", remoteStopDelayMs);
    
    // Close preferences
    preferences.end();
}

void saveGlobalSettings() {
    Serial.println("💾 Saving global LoRa settings...");
    Serial.printf("  Frequency: %.1f MHz\n", loraFrequency);
    Serial.printf("  Power: %d dBm\n", loraPower);
    Serial.printf("  SF: %d, CR: %d, BW: %.1f kHz\n", loraSpreadingFactor, loraCodingRate, loraBandwidth);
    
    // Begin preferences with "riiwynch" namespace, read-write mode
    if (!preferences.begin("riiwynch", false)) {
        Serial.println("⚠️ Failed to initialize Preferences for global settings");
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
        Serial.println("✅ Global settings successfully saved to preferences");
    } else {
        Serial.println("⚠️ Failed to save one or more global settings to preferences.");
    }
}

void loadStats() {
    if (!preferences.begin("riiwynch", false)) {
        Serial.println("⚠️ Failed to initialize Preferences for stats");
        return;
    }
    totalStarts = preferences.getULong("stats_starts", 0);
    totalRuntimeSeconds = preferences.getULong("stats_runtime", 0);
    preferences.end();
    Serial.printf("📊 Loaded Stats - Starts: %lu, Runtime: %lu sec\n", totalStarts, totalRuntimeSeconds);
}

void saveStats() {
    if (!preferences.begin("riiwynch", false)) { // open in read-write
        Serial.println("⚠️ Failed to initialize Preferences for stats");
        return;
    }
    preferences.putULong("stats_starts", totalStarts);
    preferences.putULong("stats_runtime", totalRuntimeSeconds);
    preferences.end();
    Serial.println("📊 Stats saved.");
}

void incrementStarts() {
    totalStarts++;
    saveStats();
}

void addRuntime(unsigned long seconds) {
    totalRuntimeSeconds += seconds;
    saveStats();
}

void resetStats() {
    totalStarts = 0;
    totalRuntimeSeconds = 0;
    saveStats();
    Serial.println("📊 Stats have been reset.");
}

// Wrapper functions to interface with ProfileManager
int getCurrentProfile() {
    return getGlobalProfileManager().getCurrentProfile();
}

bool isManualMode() {
    return getGlobalProfileManager().isManualMode();
}
