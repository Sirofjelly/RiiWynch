#include <WiFi.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include "Buttons.h"
#include "Relays.h"
#include "Servos.h"
#include "StartupManager.h"
#include "WebUI.h"
#include "Settings.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "StateManager.h"

DisplayManager display;
StateManager state;
ButtonManager upButton(7, &state, true);
ButtonManager downButton(40, &state, false);

unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 10;

void setup() {
  Serial.begin(115200);
  setupButtons();
  setupRelays();
  setupServos();
  loadSettings();
  setupStartup();
  setupWebUI();
  display.begin();
  display.update(state.getDisplayedPercentage());
}

void loop() {
  bool startPressed = isStartPressed();
  bool stopPressed  = isStopPressed();
  bool chokePressed = isChokePressed();
  bool brakePressed = isBrakePressed();

  upButton.update();
  downButton.update();

  // Smooth display update using millis()
  unsigned long now = millis();
  if (now - lastDisplayUpdate >= displayUpdateInterval) {
    if (state.needsDisplayUpdate()) {
      state.updateDisplayStep();
      display.update(state.getDisplayedPercentage());
    }
    lastDisplayUpdate = now;
  }

  updateRelays(startPressed, stopPressed);
  updateServos(chokePressed, brakePressed);
  updateStartup(startPressed, stopPressed);
  handleWebUI();

  delay(5); // small delay for stability
}

// Hook into StartupManager's state accessor
int getTargetPercentageFromState() {
  return state.getTargetPercentage();
}

// Provide function reference for StartupManager.cpp
StateManager& getGlobalStateManager() {
  return state;
}
