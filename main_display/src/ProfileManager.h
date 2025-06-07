#pragma once
#include <Arduino.h>
#include "StateManager.h"
#include "DisplayManager.h"
#include <RiiWynchInput/Button.h>

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
    void showModeOnReconnect(); // Show current mode when remote reconnects
    
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
    
    void loadProfile(int profileIndex);
    void showModeDisplay();
}; 