#pragma once
#include <Arduino.h>

class StateManager_remote {
public:
    enum class State { IDLE, ARMING, CRUISING, MENU };

    StateManager_remote();

    State getState() const;
    void switchToIdle();
    void switchToArming();
    void switchToCruising();
    void switchToMenu();

    int getTargetPercentage() const;
    int getShownPercentage() const;
    
    void setTargetPercentage(int percentage);
    void increasePercentage(int step);
    void decreasePercentage(int step);

    bool updateShownPercentage(int smooth_step, unsigned long interval);
    void resetMenuActivityTimer();
    bool isMenuTimedOut(unsigned long timeout) const;

private:
    State _currentState;
    int _targetPercentage;
    int _shownPercentage;
    unsigned long _lastActivityTime;
    unsigned long _lastAnimationTime;
}; 