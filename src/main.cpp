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

  // --- Mode Switching Logic ---
  static bool modeChangeComboActive = false;
  static unsigned long modeChangeDebounceTime = 0;
  const unsigned long MODE_CHANGE_DEBOUNCE = 500; // ms debounce after change

  bool upHeld = upButton.isPressed();
  bool downHeld = downButton.isPressed();
  bool currentCombo = upHeld && downHeld && stopPressed;

  if (currentCombo && !modeChangeComboActive && (millis() - modeChangeDebounceTime > MODE_CHANGE_DEBOUNCE)) {
      manualMode = !manualMode;
      modeChangeComboActive = true;
      modeChangeDebounceTime = millis(); // Start debounce timer

      // Display mode change confirmation
      if (manualMode) {
          display.updateText("MANUAL");
      } else {
          display.updateText("AUTO");
      }
      // Keep message for a bit, then restore normal display
      // Note: This simple delay might interfere with other timing.
      // A non-blocking approach using millis() would be better for complex apps.
      delay(1000); // Show message for 1 second
      // Force display refresh after mode message
      if (!stopPressed) { // Only refresh if stop is not currently pressed (otherwise it handles its own flashing)
          display.update(state.getDisplayedPercentage());
      }
  } else if (!currentCombo) {
      modeChangeComboActive = false; // Reset flag when combo is released
  }
  // --- End Mode Switching Logic ---

  // Always update button states regardless of mode
  upButton.update();
  downButton.update();

  // --- Debugging Prints ---
  // Serial.print("Up:"); Serial.print(upHeld); Serial.print(" Dn:"); Serial.print(downHeld); Serial.print(" Stop:"); Serial.print(stopPressed);
  // Serial.print(" Combo:"); Serial.print(currentCombo); Serial.print(" Active:"); Serial.print(modeChangeComboActive);
  // Serial.print(" Mode:"); Serial.print(manualMode ? "MAN" : "AUTO");
  // Serial.println();
  // --- End Debugging Prints ---

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

    // Screen updates based on mode
    if (!manualMode) {
        // Automatic mode updates
        // upButton.update(); // Moved up
        // downButton.update(); // Moved up
        if (state.needsDisplayUpdate()) {
            state.updateDisplayStep();
            display.update(state.getDisplayedPercentage());
        }
    } else {
        // Manual mode updates (if any needed besides mode display)
        // TODO: Add manual mode control logic here if needed
        // Example: maybe display something different constantly?
        // display.updateText("MANUAL ACTIVE"); // Example
    }
  }

  delay(5);
}
