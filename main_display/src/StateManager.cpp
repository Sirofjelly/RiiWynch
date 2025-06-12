#include "StateManager.h"
#include <Arduino.h>
#include "Settings.h"

void StateManager::setState(State newState) {
    if (currentState != newState) {
        currentState = newState;
        stateEnterTime = millis();
        if (newState == State::STOPPED) {
            setDirectPercentage(0);
        }
    }
}

StateManager::State StateManager::getState() const {
    return currentState;
}

void StateManager::update() {
    if (currentState == State::STOPPED) {
        if (millis() - stateEnterTime >= 5000) {
            setState(State::IDLE);
        }
    } else if (currentState == State::RUNNING) {
        if (millis() - lastRuntimeUpdateTime >= 1000) {
            addRuntime(1);
            lastRuntimeUpdateTime = millis();
        }
    }
}

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


void StateManager::start() {
    if (currentState == State::IDLE) {
        setState(State::RUNNING);
        incrementStarts();
        lastRuntimeUpdateTime = millis();
    }
}

void StateManager::stop() {
    if (currentState == State::RUNNING) {
        setState(State::STOPPED);
    }
}

