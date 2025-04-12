#include "ButtonManager.h"

ButtonManager::ButtonManager(int pin, StateManager* stateMgr, bool increase)
  : pin(pin), state(stateMgr), increaseAction(increase) {
  pinMode(pin, INPUT_PULLUP);
}

void ButtonManager::update() {
  int reading = digitalRead(pin);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentState) {
      currentState = reading;
      if (currentState == LOW) {
        pressStartTime = millis();
        lastChangeTime = millis();
      } else {
        if (millis() - pressStartTime < holdDelay) {
          if (increaseAction) state->increase();
          else state->decrease();
        }
        lastChangeTime = 0;
      }
    }
  }

  if (currentState == LOW && (millis() - pressStartTime >= holdDelay)) {
    if (millis() - lastChangeTime >= repeatInterval) {
      if (increaseAction) state->increase();
      else state->decrease();
      lastChangeTime = millis();
    }
  }

  lastReading = reading;
}

bool ButtonManager::isPressed() const {
  return currentState == LOW;
}