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
    displayedPercentage = percentage;
    displayUpdateRequested = true;
    lastUpdateTime = millis();
}

void StateManager::setDirectPercentage(int percentage) {
    targetPercentage = percentage;
    displayedPercentage = percentage;
    displayUpdateRequested = true;
    lastUpdateTime = millis();
}

void StateManager::increase() {
  if (targetPercentage < 100) {
    targetPercentage += 5;
    displayedPercentage = targetPercentage;
    displayUpdateRequested = true;
    lastUpdateTime = millis();
  }
}

void StateManager::decrease() {
  if (targetPercentage > 0) {
    targetPercentage -= 5;
    displayedPercentage = targetPercentage;
    displayUpdateRequested = true;
    lastUpdateTime = millis();
  }
}

bool StateManager::needsDisplayUpdate() {
  return displayUpdateRequested || (targetPercentage != displayedPercentage);
}

void StateManager::updateDisplayStep() {
  displayedPercentage = targetPercentage;
  displayUpdateRequested = false;
  lastUpdateTime = millis();
}

// Emergency Stop methods
void StateManager::setEmergencyStop(bool active) {
    emergencyStopActive = active;
}

bool StateManager::isEmergencyStopActive() const {
    return emergencyStopActive;
}

