#include <Preferences.h>
#include "Servos.h"
#include "StartupManager.h"
#include "Relays.h"
#include "StateManager.h"

// Forward declarations of functions
void loadSettings();
void saveSettings();
void loadSettingsForProfile(int profileIndex);
void saveSettingsForProfile(int profileIndex);

// Create a preferences instance
Preferences preferences;

unsigned long starterRelayTime = 1500;
unsigned long rampUpDuration = 2000;
float rampUpExponent = 3.00;
unsigned long rampDownDuration = 1000;
int gasIdleAngle = 15;
int gasMaxAngle = 65;
int chokeAngle = 55;
int brakeAngle = 55;
unsigned long stopCooldownDuration = 3000;
bool manualMode = false;
StateManager state;

// Define global variables for profile management
const int totalProfiles = 4; // Updated to 4 profiles

// Ensure `currentProfile` is defined only here
int currentProfile = 0;

void loadSettings() {
  // Begin preferences with "riiwynch" namespace in read-write mode
  if (!preferences.begin("riiwynch", false)) {
    Serial.println("⚠️ Failed to initialize Preferences");
    delay(1000);
    return;
  }
  
  // Load settings for the current profile
  loadSettingsForProfile(currentProfile);
  
  // Close the preferences when done
  preferences.end();
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
        rampUpDuration       = 2000;
        rampUpExponent       = 3.00;
        rampDownDuration     = 1000;
        gasIdleAngle         = 15;
        gasMaxAngle          = 65;
        chokeAngle           = 55;
        brakeAngle           = 55;
        stopCooldownDuration = 3000;
        manualMode           = false;
        
        // Save these defaults to this profile's section
        saveSettingsForProfile(profileIndex);
        preferences.end();
        return;
    }

    // Cache the old values for comparison
    unsigned long oldStarterTime = starterRelayTime;
    int oldGasIdle = gasIdleAngle;
    
    // Create keys for this profile
    String baseKey = "p" + String(profileIndex) + "_";
    
    // Load settings from this profile's section
    starterRelayTime     = preferences.getULong(String(baseKey + "starter").c_str(), 1500);
    rampUpDuration       = preferences.getULong(String(baseKey + "rampup").c_str(), 2000);
    rampUpExponent       = preferences.getFloat(String(baseKey + "rampexp").c_str(), 3.0);
    rampDownDuration     = preferences.getULong(String(baseKey + "rampdown").c_str(), 1000);
    gasIdleAngle         = preferences.getInt(String(baseKey + "gasidle").c_str(), 15);
    gasMaxAngle          = preferences.getInt(String(baseKey + "gasmax").c_str(), 65);
    chokeAngle           = preferences.getInt(String(baseKey + "choke").c_str(), 55);
    brakeAngle           = preferences.getInt(String(baseKey + "brake").c_str(), 55);
    stopCooldownDuration = preferences.getULong(String(baseKey + "cooldown").c_str(), 3000);
    manualMode           = preferences.getBool(String(baseKey + "manual").c_str(), false);
    
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
    ok &= preferences.putULong(String(baseKey + "rampup").c_str(), rampUpDuration);
    ok &= preferences.putFloat(String(baseKey + "rampexp").c_str(), rampUpExponent);
    ok &= preferences.putULong(String(baseKey + "rampdown").c_str(), rampDownDuration);
    ok &= preferences.putInt(String(baseKey + "gasidle").c_str(), gasIdleAngle);
    ok &= preferences.putInt(String(baseKey + "gasmax").c_str(), gasMaxAngle);
    ok &= preferences.putInt(String(baseKey + "choke").c_str(), chokeAngle);
    ok &= preferences.putInt(String(baseKey + "brake").c_str(), brakeAngle);
    ok &= preferences.putULong(String(baseKey + "cooldown").c_str(), stopCooldownDuration);
    ok &= preferences.putBool(String(baseKey + "manual").c_str(), manualMode);

    preferences.end();

    if (ok) {
        Serial.println("✅ Settings successfully saved to preferences");
    } else {
        Serial.println("⚠️ Failed to save one or more settings to preferences. Possible NVS corruption or out of space.");
    }
}
