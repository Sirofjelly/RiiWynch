#include "ProfileManager.h"
#include "Settings.h"  // For loadSettingsForProfile() and global variables
#include <Arduino.h>

// Static member definitions
const char* ProfileManager::modeNames[4] = {"SURF", "SKIM", "SMOOTH", "MANUAL"};

ProfileManager::ProfileManager(StateManager& stateMgr, DisplayManager& displayMgr, Button& up, Button& down)
    : state(stateMgr), display(displayMgr), upButton(up), downButton(down), currentProfileIndex(0) {
}

void ProfileManager::begin() {
    // Initialize with Auto 1 profile
    currentProfile = 0; // Always start in Auto 1
    manualMode = false;
    modeState = 0;
    loadSettingsForProfile(currentProfile); // Load profile 1 settings
}

void ProfileManager::update() {
    // Update mode display (non-blocking timeout handling)
    display.updateModeDisplay();
    
    // Always update button states regardless of mode
    checkModeSwitch(false);
}

void ProfileManager::checkModeSwitch(bool stopPressed) {
    // Check if both buttons are held down to switch modes
    if (upButton.isPressed() && downButton.isPressed()) {
        if (!upButtonWasPressed || !downButtonWasPressed) {
            // Both buttons have just been pressed, start the timer
            lastButtonCheckTime = millis();
            upButtonWasPressed = true;
            downButtonWasPressed = true;
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
    }
}

void ProfileManager::switchToNextMode() {
    modeState = (currentProfile + 1) % 4; // Synchronize modeState with currentProfile
    manualMode = (modeState == 3);
    currentProfile = modeState;
    loadSettingsForProfile(currentProfile);

    // Display mode change confirmation using new protected display method
    display.startModeDisplay(modeNames[modeState], 2000); // Show for 2 seconds
    
    if (manualMode) {
        state.setTargetPercentage(5);
    }
    
    Serial.printf("Mode switched to: %s (Profile %d)\n", modeNames[modeState], currentProfile + 1);
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

void ProfileManager::showModeOnReconnect() {
    if (manualMode) {
        display.startModeDisplay(modeNames[3], 1500); // MANUAL - show for 1.5 seconds
    } else {
        display.startModeDisplay(modeNames[currentProfile], 1500); // Show current mode for 1.5 seconds
    }
}

void ProfileManager::loadProfile(int profileIndex) {
    loadSettingsForProfile(profileIndex);
}