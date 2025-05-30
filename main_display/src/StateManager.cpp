#include "StateManager.h"
#include <Arduino.h>

int StateManager::getTargetPercentage() const {
    return targetPercentage;
}

int StateManager::getDisplayedPercentage() const {
    return displayedPercentage;
}

void StateManager::setTargetPercentage(int percentage) {
    targetPercentage = percentage;
}

void StateManager::setDirectPercentage(int percentage) {
    targetPercentage = percentage;
    displayedPercentage = percentage;
    lastUpdateTime = millis(); // Reset timer to prevent immediate animation
}

void StateManager::increase() {
  if (targetPercentage < 100) targetPercentage += 5;
}

void StateManager::decrease() {
  if (targetPercentage > 0) targetPercentage -= 5;
}

bool StateManager::needsDisplayUpdate() {
  return millis() - lastUpdateTime > updateInterval &&
         targetPercentage != displayedPercentage;
}

void StateManager::updateDisplayStep() {
  if (displayedPercentage < targetPercentage) displayedPercentage++;
  else if (displayedPercentage > targetPercentage) displayedPercentage--;
  lastUpdateTime = millis();
}

