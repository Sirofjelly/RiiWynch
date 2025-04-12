#include <WiFi.h>
#include <WebServer.h>
#include "Buttons.h"
#include "Relays.h"
#include "Servos.h"
#include "StartupManager.h"
#include "WebUI.h"
#include "DisplayManager.h"
#include "StateManager.h"
#include "ButtonManager.h"
#include "Settings.h"  // 🆕 Include for loadSettings()

DisplayManager display;
StateManager state;
ButtonManager upButton(7, &state, true);
ButtonManager downButton(40, &state, false);

bool manualMode = false;

StateManager& getGlobalStateManager() {
  return state;
}

void setup() {
  Serial.begin(115200);
  loadSettings();       // 🟢 Load saved settings first
  setupButtons();
  setupRelays();
  setupServos();
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

  updateRelays(startPressed, stopPressed);
  updateServos(chokePressed, brakePressed);

  if (startupInProgress || state.getTargetPercentage() == 0) {
    startPressed = false;
  }

  updateStartup(startPressed, stopPressed);
  handleWebUI();

  // STOP blinking
  static bool flashState = false;
  static unsigned long lastFlashTime = 0;
  static bool wasStopFlashing = false;

  if (stopPressed) {
    unsigned long now = millis();
    if (now - lastFlashTime > 300) {
      flashState = !flashState;
      lastFlashTime = now;

      if (flashState) {
        display.updateText("STOP");
      } else {
        display.clear();
      }
    }
    wasStopFlashing = true;
  } else {
    // 🛠️ Fix: recover display after stop is released
    if (wasStopFlashing) {
      display.update(state.getDisplayedPercentage());
      wasStopFlashing = false;
    }

    // Normal screen updates
    upButton.update();
    downButton.update();

    if (state.needsDisplayUpdate()) {
      state.updateDisplayStep();
      display.update(state.getDisplayedPercentage());
    }
  }

  delay(5);
}
