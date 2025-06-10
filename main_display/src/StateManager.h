#pragma once

class StateManager {
public:
    enum class State {
        IDLE,
        RUNNING,
        STOPPED
    };

    void setState(State newState);
    State getState() const;
    void start();
    void stop();

    void increase();
    void decrease();
    bool needsDisplayUpdate();
    void updateDisplayStep();
    int getTargetPercentage() const;
    int getDisplayedPercentage() const;
    void setTargetPercentage(int percentage);
    void setDirectPercentage(int percentage); // Set both target and displayed immediately
    void update(); // Add an update method for state transitions

private:
    State currentState = State::IDLE;
    unsigned long stateEnterTime = 0;

    int targetPercentage;
    int displayedPercentage = 0;
    unsigned long lastUpdateTime = 0;
    const unsigned long updateInterval = 10;
    bool displayUpdateRequested = false; // Flag to trigger display updates
};