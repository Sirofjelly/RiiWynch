#include "StateManager.h"
#include <Arduino.h>
#include "Settings.h"
#include "LoRaManager.h"

void StateManager::setState(State newState) {
    if (currentState != newState) {
        currentState = newState;
        stateEnterTime = millis();
        if (newState == State::STOPPED) {
            // Notify remote that the motor has stopped while keeping the last set percentage
            extern LoRaManager& getGlobalLoRaManager();
            getGlobalLoRaManager().sendStopMotor();
        }
    }
}

StateManager::State StateManager::getState() const {
    return currentState;
}

void StateManager::update() {
    // Handle timeout for remote stops
    if (currentState == State::STOPPED && hasTimeout) {
        if (millis() - stateEnterTime >= timeoutDuration) {
            setState(State::IDLE);
            hasTimeout = false; // Reset timeout flag
        }
    }
    
    if (currentState == State::RUNNING) {
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
    int safePercentage = constrain(percentage, 0, 100);
    targetPercentage = safePercentage;
    displayedPercentage = safePercentage;
    displayUpdateRequested = true;
    lastUpdateTime = millis();
}

void StateManager::setDirectPercentage(int percentage) {
    int safePercentage = constrain(percentage, 0, 100);
    targetPercentage = safePercentage;
    displayedPercentage = safePercentage;
    displayUpdateRequested = true;
    lastUpdateTime = millis();
}

void StateManager::increase() {
  if (targetPercentage < 100) {
    targetPercentage += 1;  // Changed from 5 to 1 to match remote behavior
    displayedPercentage = targetPercentage;
    displayUpdateRequested = true;
    lastUpdateTime = millis();
  }
}

void StateManager::decrease() {
  if (targetPercentage > 0) {
    targetPercentage = max(0, targetPercentage - 1);  // Changed from 5 to 1 to match remote behavior
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
        hasTimeout = false; // Local stops don't have timeout
    }
}

void StateManager::shouldStayStopped(bool stopButtonPressed) {
    // This method is called when we want to maintain the STOPPED state
    // As long as stop button is pressed, we stay in STOPPED state
    // If not pressed and we're in STOPPED state, this indicates the button was released
    if (currentState == State::STOPPED && !stopButtonPressed) {
        // Stop button was released, exit stop state
        exitStop();
    }
}

void StateManager::exitStop() {
    if (currentState == State::STOPPED) {
        setState(State::IDLE);
        hasTimeout = false; // Reset timeout flag when manually exiting
    }
}

void StateManager::stopWithTimeout(unsigned long timeoutMs) {
    if (currentState == State::RUNNING) {
        setState(State::STOPPED);
        hasTimeout = true;
        timeoutDuration = timeoutMs;
    }
}

