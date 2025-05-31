#pragma once

class StateManager {
public:
    void increase();
    void decrease();
    bool needsDisplayUpdate();
    void updateDisplayStep();
    int getTargetPercentage() const;               
    int getDisplayedPercentage() const;           
    void setTargetPercentage(int percentage); 
    void setDirectPercentage(int percentage); // Set both target and displayed immediately

    // Emergency Stop
    void setEmergencyStop(bool active);
    bool isEmergencyStopActive() const;

private:
  int targetPercentage;
  int displayedPercentage = 0;
  unsigned long lastUpdateTime = 0;
  const unsigned long updateInterval = 10;
  bool displayUpdateRequested = false; // Flag to trigger display updates
  bool emergencyStopActive = false; // Flag for emergency stop
};