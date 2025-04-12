#pragma once

class StateManager {
public:
  int getTargetPercentage() const;
  int getDisplayedPercentage() const;
  void increase();
  void decrease();
  bool needsDisplayUpdate();
  void updateDisplayStep();
private:
  int targetPercentage = 0;
  int displayedPercentage = 0;
  unsigned long lastUpdateTime = 0;
  const unsigned long updateInterval = 10;
};