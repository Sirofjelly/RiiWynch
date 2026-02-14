#include "ProfileManager.h"
#include "Settings.h"  // For loadSettingsForProfile() and global variables
#include "LoRaManager.h" // 🔄 For sending mode updates to remote
extern LoRaManager& getGlobalLoRaManager(); // 🔄 Forward declaration
#include <Arduino.h>

// Static member definitions
const char* ProfileManager::modeNames[4] = {"SURF", "SKIM", "SMOOTH", "MANUAL"};

ProfileManager::ProfileManager(StateManager& stateMgr, DisplayManager& displayMgr, Button& up, Button& down)
    : state(stateMgr), display(displayMgr), upButton(up), downButton(down), currentProfileIndex(0) {
}

void ProfileManager::begin() {
    // Initialize with Auto 1 profile
    modeState = 0;
    currentProfile = 0; // Always start in Auto 1
    manualMode = false;
    inModeSwitchState = false; // Initialize the flag
    
    // Sync global variables to match ProfileManager state
    ::currentProfile = this->currentProfile;
    ::manualMode = this->manualMode;
    
    loadSettingsForProfile(currentProfile); // Load profile 1 settings
}

void ProfileManager::update() {
    // Update mode display (non-blocking timeout handling)
    display.updateModeDisplay();
}

void ProfileManager::checkModeSwitch(bool stopPressed) {
    // Check if both buttons are held down to switch modes
    if (upButton.isPressed() && downButton.isPressed()) {
        if (!upButtonWasPressed || !downButtonWasPressed) {
            // Both buttons have just been pressed, start the timer
            lastButtonCheckTime = millis();
            upButtonWasPressed = true;
            downButtonWasPressed = true;
            inModeSwitchState = true; // Set the flag when both buttons are pressed
        } else {
            // Both buttons are still being held, check if hold time has passed
            if (millis() - lastButtonCheckTime >= MODE_SWITCH_HOLD_TIME) {
                switchToNextMode();
                // Reset time to allow for continuous cycling if held
                lastButtonCheckTime = millis();
            }
        }
    } else {
        // If either button is released, reset the state
        upButtonWasPressed = false;
        downButtonWasPressed = false;
        inModeSwitchState = false; // Clear the flag when buttons are released
    }
}

bool ProfileManager::isInModeSwitchState() const {
    return inModeSwitchState;
}

void ProfileManager::switchToNextMode() {
    modeState = (modeState + 1) % 4;
    manualMode = (modeState == 3);
    currentProfile = modeState;
    
    // Sync global variables
    ::currentProfile = this->currentProfile;
    ::manualMode = this->manualMode;
    
    loadSettingsForProfile(currentProfile);

    // Display mode change confirmation using new protected display method
    display.startModeDisplay(modeNames[modeState], 2000); // Show for 2 seconds
    
    if (manualMode) {
        state.setTargetPercentage(5);
    }
    
    Serial.printf("Mode switched to: %s (Profile %d)\n", modeNames[modeState], currentProfile + 1);

    // 🔄 Notify remote about mode change
    getGlobalLoRaManager().sendModeUpdate(static_cast<uint8_t>(modeState));
}

// New API methods for WebUI integration
void ProfileManager::setProfile(int profileIndex) {
    if (profileIndex < 0 || profileIndex > 3) {
        Serial.printf("Invalid profile index: %d\n", profileIndex);
        return;
    }
    
    modeState = profileIndex;
    currentProfile = profileIndex;
    manualMode = (profileIndex == 3);
    
    // Sync global variables
    ::currentProfile = this->currentProfile;
    ::manualMode = this->manualMode;
    
    loadSettingsForProfile(currentProfile);
    display.startModeDisplay(modeNames[modeState], 1500);
    
    if (manualMode) {
        state.setTargetPercentage(5);
    }
    
    Serial.printf("Profile set to: %s (Profile %d)\n", modeNames[modeState], currentProfile + 1);

    // 🔄 Notify remote about mode change
    getGlobalLoRaManager().sendModeUpdate(static_cast<uint8_t>(modeState));
}

void ProfileManager::setManualMode(bool manual) {
    manualMode = manual;
    modeState = manual ? 3 : currentProfile;
    
    // Sync global variables
    ::manualMode = this->manualMode;
    
    if (manual) {
        currentProfile = 3;
        ::currentProfile = 3;
        state.setTargetPercentage(5);
        display.startModeDisplay(modeNames[3], 1500);
    } else {
        // When exiting manual mode, go back to the last auto profile
        if (currentProfile == 3) {
            currentProfile = 0; // Default to SURF if we were in manual
        }
        modeState = currentProfile;
        ::currentProfile = this->currentProfile;
        display.startModeDisplay(modeNames[currentProfile], 1500);
    }
    
    loadSettingsForProfile(currentProfile);
    Serial.printf("Manual mode %s, Profile: %s\n", manual ? "enabled" : "disabled", modeNames[modeState]);

    // 🔄 Notify remote about mode change
    getGlobalLoRaManager().sendModeUpdate(static_cast<uint8_t>(modeState));
}

void ProfileManager::cycleProfile() {
    // Save current profile settings before switching
    saveSettingsForProfile(currentProfile);
    Serial.printf("💾 Saved settings to profile %d before switching\n", currentProfile + 1);
    
    // Cycle to next profile
    switchToNextMode();
}

int ProfileManager::getCurrentProfile() const {
    return currentProfile;
}

const char* ProfileManager::getCurrentModeName() const {
    return modeNames[modeState];
}

bool ProfileManager::isManualMode() const {
    return manualMode;
}

void ProfileManager::loadProfile(int profileIndex) {
    loadSettingsForProfile(profileIndex);
}
