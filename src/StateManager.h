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

private:
  int targetPercentage;
  int displayedPercentage = 0;
  unsigned long lastUpdateTime = 0;
  const unsigned long updateInterval = 10;
};