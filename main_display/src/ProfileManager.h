#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include "StateManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"

class ProfileManager {
public:
    ProfileManager(StateManager& stateMgr, DisplayManager& displayMgr, ButtonManager& upBtn, ButtonManager& downBtn);
    
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
    ButtonManager& upButton;
    ButtonManager& downButton;
    
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

#endif // PROFILE_MANAGER_H 