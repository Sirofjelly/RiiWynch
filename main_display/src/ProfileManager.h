#pragma once
#include <Arduino.h>
#include "StateManager.h"
#include "DisplayManager.h"
#include "Button.h"

class ProfileManager {
public:
    ProfileManager(StateManager& stateMgr, DisplayManager& displayMgr, Button& up, Button& down);
    
    void begin();
    void update();
    void switchToNextMode();
    int getCurrentProfile() const;
    const char* getCurrentModeName() const;
    bool isManualMode() const;
    void checkModeSwitch(bool stopPressed);
    
    // New API methods for WebUI integration
    void setProfile(int profileIndex);  // Set specific profile directly
    void setManualMode(bool manual);    // Toggle manual mode
    void cycleProfile();                // Cycle to next profile (for WebUI)
    
private:
    // References to other managers
    StateManager& state;
    DisplayManager& display;
    Button& upButton;
    Button& downButton;
    
    // Mode/Profile state
    int modeState; // 0: Auto 1, 1: Auto 2, 2: Auto 3, 3: Manual
    static const char* modeNames[4];
    
    // Profile switching logic
    bool profileSwitchComboActive;
    unsigned long profileSwitchDebounceTime;
    static const unsigned long PROFILE_SWITCH_DEBOUNCE = 500; // ms

    // === MISSING MEMBERS ADDED ===
    int currentProfileIndex; // Used in constructor
    int currentProfile;      // Used for profile tracking
    bool manualMode;         // Used for manual mode tracking
    bool upButtonWasPressed = false;
    bool downButtonWasPressed = false;
    unsigned long lastButtonCheckTime = 0;
    static const unsigned long MODE_SWITCH_HOLD_TIME = 2000; // ms, adjust as needed
    // =============================

    void loadProfile(int profileIndex);
    void showModeDisplay();
}; 