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
#include <RadioLib.h>

DisplayManager display;
ButtonManager upButton(7, &state, true);
ButtonManager downButton(40, &state, false);

// Global mode state for cycling
static int modeState = 0; // 0: Auto 1, 1: Auto 2, 2: Auto 3, 3: Manual
const char* modeNames[4] = {"SURF", "SKIM", "SMOOTH", "MANUAL"};

StateManager& getGlobalStateManager() {
  return state;
}

// SX1262 LoRa pinout (same as remote)
#define L_CS   8
#define L_DIO1 14
#define L_RST  12
#define L_BUSY 13
Module mod(L_CS, L_DIO1, L_RST, L_BUSY, SPI);
SX1262 radio(&mod);

// Buffer for received LoRa messages
char loraRxBuf[64];
unsigned long lastLoraMsgTime = 0;
bool loraMsgActive = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Setup.");
  currentProfile = 0; // Always start in Auto 1
  manualMode = false;
  modeState = 0;
  loadSettingsForProfile(currentProfile); // Load profile 1 settings
  setupButtons();
  setupRelays();
  setupServos();
  setupStartup();
  setupWebUI();
  display.begin();
  display.update(state.getDisplayedPercentage());
  
  // Enable LoRa power (VEXT)
  #define VEXT 21
  pinMode(VEXT, OUTPUT);
  digitalWrite(VEXT, LOW); // Enable LoRa power

  // Use correct SPI pins for Heltec WiFi LoRa 32 (V3)
  SPI.begin(9, 11, 10, 8); // SCK, MISO, MOSI, SS
  int err = radio.begin(868.0);
  if (err != RADIOLIB_ERR_NONE) {
    Serial.print("LoRa init failed: ");
    Serial.println(err);
  } else {
    radio.setOutputPower(14);
    radio.setSpreadingFactor(8);
    radio.setCodingRate(5);
    radio.setBandwidth(125.0);
    Serial.println("LoRa ready.");
  }
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
  // (Removed: old manualMode toggle logic. All switching is now handled by the profile/mode cycling logic below)
  // --- End Mode Switching Logic ---

  // Profile/mode switching logic
  static bool profileSwitchComboActive = false;
  static unsigned long profileSwitchDebounceTime = 0;
  const unsigned long PROFILE_SWITCH_DEBOUNCE = 500; // ms debounce after change

  bool upHeld = upButton.isPressed();
  bool downHeld = downButton.isPressed();
  bool currentCombo = upHeld && downHeld && stopPressed;

  // Fixed logic to ensure "Smooth" profile is not skipped
  // Ensure button logic synchronizes with Web UI profile changes
  if (currentCombo && !profileSwitchComboActive && (millis() - profileSwitchDebounceTime > PROFILE_SWITCH_DEBOUNCE)) {
      modeState = (currentProfile + 1) % 4; // Synchronize modeState with currentProfile
      manualMode = (modeState == 3);
      currentProfile = modeState;
      loadSettingsForProfile(currentProfile);

      profileSwitchComboActive = true;
      profileSwitchDebounceTime = millis();

      // Display mode change confirmation
      display.updateText(modeNames[modeState]);
      if (manualMode) {
          state.setTargetPercentage(5);
      }
      delay(1000);
      display.update(state.getDisplayedPercentage());
  } else if (!currentCombo) {
      profileSwitchComboActive = false;
  }

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
    if (now - lastFlashTime > 600) { // slower blink
      flashState = !flashState;
      lastFlashTime = now;
    }
    if (flashState) {
      display.blinkStopText(true); // show STOP text only
    } else {
      display.blinkStopText(false); // hide STOP text, keep frame/bar
    }
    wasStopFlashing = true;
  } else {
    // 🛠️ Fix: recover display after stop is released
    if (wasStopFlashing) {
      display.update(state.getDisplayedPercentage());
      wasStopFlashing = false;
    }

    // Screen updates - Now happens in both modes
    if (state.needsDisplayUpdate()) {
        state.updateDisplayStep();
        display.update(state.getDisplayedPercentage());
    }
    /* OLD Logic - commented out
    // Screen updates based on mode
    if (!manualMode) {
        // Automatic mode updates
        if (state.needsDisplayUpdate()) {
            state.updateDisplayStep();
            display.update(state.getDisplayedPercentage());
        }
    } else {
        // Manual mode updates (if any needed besides mode display)
        // Display update is now handled above, regardless of mode.
    }
    */
  }

  // In MANUAL_CONTROL state, always update servo to match percentage (auto and manual mode)
  if (currentState == MANUAL_CONTROL) {
    gasServo.write(calculateTargetAngle());
  }

  // --- LoRa receive logic ---
  int loraStatus = radio.receive((uint8_t*)loraRxBuf, sizeof(loraRxBuf) - 1);
  if (loraStatus == RADIOLIB_ERR_NONE) {
    loraRxBuf[sizeof(loraRxBuf) - 1] = '\0'; // Ensure null-terminated
    Serial.print("[LoRa RX] ");
    Serial.println(loraRxBuf);
  }

  delay(5);
}
