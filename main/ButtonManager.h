#pragma once
#include <Arduino.h>
#include "StateManager.h"

class ButtonManager {
public:
  ButtonManager(int pin, StateManager* stateMgr, bool increase);
  void update();
private:
  int pin;
  bool increaseAction;
  StateManager* state;
  int lastReading = HIGH;
  int currentState = HIGH;
  unsigned long lastDebounceTime = 0;
  unsigned long pressStartTime = 0;
  unsigned long lastChangeTime = 0;
  const unsigned long debounceDelay = 20;
  const unsigned long holdDelay = 500;
  const unsigned long repeatInterval = 100;
};