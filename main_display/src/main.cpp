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

// === PROFILING SYSTEM ===
#define ENABLE_PROFILING 1  // Set to 0 to disable profiling

#if ENABLE_PROFILING
unsigned long profileLoopStart = 0;
unsigned long profileSectionStart = 0;
unsigned long profileLoopCount = 0;

#define PROFILE_LOOP_START() \
  do { \
    profileLoopStart = millis(); \
    profileSectionStart = profileLoopStart; \
    profileLoopCount++; \
  } while(0)

#define PROFILE_SECTION(name) \
  do { \
    unsigned long now = millis(); \
    unsigned long duration = now - profileSectionStart; \
    if (duration > 5) { \
      Serial.printf("[PROFILE] %s: %lu ms\n", name, duration); \
    } \
    profileSectionStart = now; \
  } while(0)

#define PROFILE_LOOP_END() \
  do { \
    unsigned long totalTime = millis() - profileLoopStart; \
    if (totalTime > 20 || (profileLoopCount % 1000 == 0)) { \
      Serial.printf("[PROFILE] Loop #%lu total: %lu ms\n", profileLoopCount, totalTime); \
    } \
  } while(0)
#else
#define PROFILE_LOOP_START()
#define PROFILE_SECTION(name)
#define PROFILE_LOOP_END()
#endif
// === END PROFILING SYSTEM ===

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

// --- LoRa Interrupt Handling ---
volatile bool loraMessageReady = false;

// LoRa interrupt handler
void IRAM_ATTR onLoRaReceive() {
  loraMessageReady = true;
}
// --- End LoRa Interrupt Handling ---

// --- Remote Connection Supervision ---
unsigned long lastRemoteHeartbeatTime = 0;
const unsigned long REMOTE_HEARTBEAT_TIMEOUT = 2000; // Milliseconds (e.g., 4x heartbeat interval of 500ms)
bool remoteConnected = false;
// --- End Remote Connection Supervision ---

// --- LoRa Sync State ---
int lastSentDisplayPct = -1;
bool waitingForAck = false;
unsigned long lastLoraSendTime = 0;
const unsigned long LORA_RESEND_INTERVAL = 200; // ms

// --- LoRa ACK helper ---
void sendLoraAck(int pct) {
    char ackBuf[16];
    snprintf(ackBuf, sizeof(ackBuf), "ACK,%d", pct);
    
    // Transmit the ACK
    int txResult = radio.transmit((uint8_t*)ackBuf, strlen(ackBuf));
    if (txResult == RADIOLIB_ERR_NONE) {
        Serial.print("[LoRa TX] ");
        Serial.println(ackBuf);
    } else {
        Serial.printf("[LoRa TX] Error: %d\n", txResult);
    }
    
    // Restart receive mode after transmission
    radio.startReceive();
}

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
  
  // Initialize remote connection status
  lastRemoteHeartbeatTime = millis(); // Initialize to current time to avoid immediate timeout
  remoteConnected = false; // Assume not connected until first heartbeat

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
    
    // Configure LoRa interrupt for non-blocking operation
    radio.setDio1Action(onLoRaReceive);
    
    // Start continuous receive mode
    int rxErr = radio.startReceive();
    if (rxErr == RADIOLIB_ERR_NONE) {
      Serial.println("LoRa ready - Interrupt mode enabled.");
    } else {
      Serial.print("LoRa startReceive failed: ");
      Serial.println(rxErr);
    }
  }
}

void loop() {
  PROFILE_LOOP_START();

  bool startPressed = isStartPressed();
  bool stopPressed  = isStopPressed();
  bool chokePressed = isChokePressed();
  bool brakePressed = isBrakePressed();
  PROFILE_SECTION("Button reads");

  updateRelays(startPressed, stopPressed);
  updateServos(chokePressed, brakePressed);
  PROFILE_SECTION("Relay/Servo updates");

  if (startupInProgress || state.getTargetPercentage() == 0) {
    startPressed = false;
  }

  updateStartup(startPressed, stopPressed);
  PROFILE_SECTION("Startup update");
  
  handleWebUI();
  PROFILE_SECTION("WebUI handling");

  // --- Remote Connection Check ---
  if (remoteConnected && (millis() - lastRemoteHeartbeatTime > REMOTE_HEARTBEAT_TIMEOUT)) {
    Serial.println("Remote connection lost! Initiating STOP sequence.");
    state.setTargetPercentage(0); // Key stop action
    // Optionally, add other direct stop actions here if needed, e.g., specific relay/servo commands
    
    display.updateText("NO REMOTE"); // Display connection lost message
    // delay(1000); // Optional delay to ensure message is seen, but loop should continue
    // display.update(state.getDisplayedPercentage()); // Update will show 0%

    remoteConnected = false; // Update status
    // No need to reset lastRemoteHeartbeatTime here, it will be updated when remote reconnects
  }
  PROFILE_SECTION("Remote connection check");

  // --- Mode Switching Logic ---
  // (Removed: old manualMode toggle logic. All switching is now handled by the profile/mode cycling logic below)
  // --- End Mode Switching Logic ---

  // Profile/mode switching logic
  static bool profileSwitchComboActive = false;
  static unsigned long profileSwitchDebounceTime = 0;
  static bool showingModeText = false;
  static unsigned long modeTextStartTime = 0;
  const unsigned long PROFILE_SWITCH_DEBOUNCE = 500; // ms debounce after change
  const unsigned long MODE_TEXT_DISPLAY_TIME = 1000; // ms to show mode text

  bool upHeld = upButton.isPressed();
  bool downHeld = downButton.isPressed();
  bool currentCombo = upHeld && downHeld && stopPressed;

  // Handle mode text display timeout (non-blocking replacement for delay(1000))
  if (showingModeText && (millis() - modeTextStartTime >= MODE_TEXT_DISPLAY_TIME)) {
      showingModeText = false;
      display.update(state.getDisplayedPercentage());
  }

  // Fixed logic to ensure "Smooth" profile is not skipped
  // Ensure button logic synchronizes with Web UI profile changes
  if (currentCombo && !profileSwitchComboActive && !showingModeText && 
      (millis() - profileSwitchDebounceTime > PROFILE_SWITCH_DEBOUNCE)) {
      modeState = (currentProfile + 1) % 4; // Synchronize modeState with currentProfile
      manualMode = (modeState == 3);
      currentProfile = modeState;
      loadSettingsForProfile(currentProfile);

      profileSwitchComboActive = true;
      profileSwitchDebounceTime = millis();

      // Display mode change confirmation (non-blocking)
      display.updateText(modeNames[modeState]);
      showingModeText = true;
      modeTextStartTime = millis();
      
      if (manualMode) {
          state.setTargetPercentage(5);
      }
  } else if (!currentCombo) {
      profileSwitchComboActive = false;
  }
  PROFILE_SECTION("Mode switching logic");

  // Always update button states regardless of mode
  upButton.update();
  downButton.update();
  PROFILE_SECTION("Button state updates");

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
        int dispPct = state.getDisplayedPercentage();
        display.update(dispPct);
        // --- LoRa sync: send displayed percentage to remote, wait for ACK ---
        if (!waitingForAck || dispPct != lastSentDisplayPct) {
            char loraTxBuf[16];
            snprintf(loraTxBuf, sizeof(loraTxBuf), "DSP,%d", dispPct);
            Serial.print("[LoRa TX to Remote] ");
            Serial.println(loraTxBuf);
            int txResult = radio.transmit((uint8_t*)loraTxBuf, strlen(loraTxBuf));
            if (txResult == RADIOLIB_ERR_NONE) {
                lastSentDisplayPct = dispPct;
                waitingForAck = true;
                lastLoraSendTime = millis();
            } else {
                Serial.printf("[LoRa TX] Error: %d\n", txResult);
            }
            // Restart receive mode after transmission
            radio.startReceive();
        } else if (waitingForAck && millis() - lastLoraSendTime > LORA_RESEND_INTERVAL) {
            // Resend if no ACK
            char loraTxBuf[16];
            snprintf(loraTxBuf, sizeof(loraTxBuf), "DSP,%d", lastSentDisplayPct);
            Serial.print("[LoRa TX to Remote - RESEND] ");
            Serial.println(loraTxBuf);
            int txResult = radio.transmit((uint8_t*)loraTxBuf, strlen(loraTxBuf));
            if (txResult == RADIOLIB_ERR_NONE) {
                lastLoraSendTime = millis();
            } else {
                Serial.printf("[LoRa TX RESEND] Error: %d\n", txResult);
            }
            // Restart receive mode after transmission
            radio.startReceive();
        }
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
  PROFILE_SECTION("Display updates & LoRa TX");

  // In MANUAL_CONTROL state, always update servo to match percentage (auto and manual mode)
  if (currentState == MANUAL_CONTROL) {
    // Only adjust gas servo if remote is connected or if connection is not required for manual adjustments
    // For safety, let's assume connection is required for any gas servo action not explicitly stop.
    if (remoteConnected || state.getTargetPercentage() == 0) { 
        gasServo.write(calculateTargetAngle());
    }
  }
  PROFILE_SECTION("Manual control servo");

  // --- LoRa receive logic (INTERRUPT-BASED NON-BLOCKING) ---
  if (loraMessageReady) {
    loraMessageReady = false; // Clear the flag
    
    // Read the received data - this is fast, no blocking
    int loraStatus = radio.readData((uint8_t*)loraRxBuf, sizeof(loraRxBuf) - 1);
    
    if (loraStatus == RADIOLIB_ERR_NONE) {
      loraRxBuf[sizeof(loraRxBuf) - 1] = '\0'; // Ensure null-terminated
      Serial.print("[LoRa RX] ");
      Serial.println(loraRxBuf);

      bool messageProcessed = false;

      // --- LoRa sync: parse VAL,<pct> from remote ---
      int pct = -1;
      if (sscanf(loraRxBuf, "VAL,%d", &pct) == 1 && pct >= 0 && pct <= 100) {
        Serial.printf("[LoRa RX] Parsed VAL: %d\n", pct); // DEBUG
        if (remoteConnected) { // Only accept VAL if remote is considered connected
            state.setTargetPercentage(pct);
            sendLoraAck(pct); // <--- Send ACK back to remote
        } else {
            Serial.println("[LoRa RX] Ignored VAL, remote not connected.");
        }
        messageProcessed = true;
      }

      // --- LoRa: parse ACK,<pct> from remote ---
      int ackPct = -1;
      if (!messageProcessed && sscanf(loraRxBuf, "ACK,%d", &ackPct) == 1) {
          Serial.printf("[LoRa RX] Got ACK for %d\n", ackPct);
          if (waitingForAck && ackPct == lastSentDisplayPct) {
              waitingForAck = false;
          }
          messageProcessed = true;
      }

      // --- LoRa: parse HBT (Heartbeat) from remote ---
      if (!messageProcessed && strncmp(loraRxBuf, "HBT,", 4) == 0) {
        Serial.println("[LoRa RX] Parsed HBT (Heartbeat)");
        messageProcessed = true;
      }

      // --- LoRa: parse BTN (Button state) from remote ---
      int btnState = -1;
      if (!messageProcessed && strncmp(loraRxBuf, "BTN,", 4) == 0) {
        Serial.printf("[LoRa RX] Parsed BTN: %s\n", loraRxBuf);
        messageProcessed = true;
      }

      if (messageProcessed) {
        if (!remoteConnected) {
          Serial.println("Remote (re)connected.");
          if (manualMode) {
              display.updateText(modeNames[3]); // MANUAL
          } else {
              display.updateText(modeNames[currentProfile]);
          }
        }
        lastRemoteHeartbeatTime = millis();
        remoteConnected = true;
      } else {
        Serial.println("[LoRa RX] Unknown message format from remote.");
      }
    } else {
      Serial.printf("[LoRa RX] Read error: %d\n", loraStatus);
    }
    
    // Restart continuous receive for next message
    radio.startReceive();
  }
  PROFILE_SECTION("LoRa receive & processing");

  PROFILE_LOOP_END();
  delay(5);
}
