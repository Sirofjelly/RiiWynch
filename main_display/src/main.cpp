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
#include "LoRaManager.h"
#include "HeartbeatManager.h"
#include "ProfileManager.h"
#include "TaskManager.h"
#include <RadioLib.h>
#include <FreeRTOS.h>
#include <semphr.h>

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

// Component instances
DisplayManager display;
ButtonManager upButton(7, &state, true);
ButtonManager downButton(40, &state, false);

// Manager instances
LoRaManager loraManager(state, display);
HeartbeatManager heartbeatManager(state, display);
ProfileManager profileManager(state, display, upButton, downButton);
TaskManager taskManager;

// Global mode state for cycling
static int modeState = 0; // 0: Auto 1, 1: Auto 2, 2: Auto 3, 3: Manual
const char* modeNames[4] = {"SURF", "SKIM", "SMOOTH", "MANUAL"};

StateManager& getGlobalStateManager() {
  return state;
}

void handleDisplayUpdates(bool stopPressed);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Setup.");
  
  // Initialize existing subsystems
  setupButtons();
  setupRelays();
  setupServos();
  setupStartup();
  setupWebUI();
  display.begin();
  display.update(state.getDisplayedPercentage());

  // Initialize managers
  Serial.println("Initializing managers...");
  
  // Initialize ProfileManager first as it sets up the initial profile
  profileManager.begin();
  
  // Initialize LoRaManager
  if (!loraManager.begin()) {
    Serial.println("Failed to initialize LoRa Manager!");
  }
  
  // Initialize HeartbeatManager
  if (!heartbeatManager.begin()) {
    Serial.println("Failed to initialize Heartbeat Manager!");
  }
  
  // Connect managers for inter-manager communication
  loraManager.setHeartbeatManager(&heartbeatManager);
  heartbeatManager.setProfileManager(&profileManager);
  
  // Initialize TaskManager and register other managers
  taskManager.begin();
  taskManager.registerLoRaManager(&loraManager);
  taskManager.registerHeartbeatManager(&heartbeatManager);
  
  Serial.println("Setup complete - All managers initialized");
}

void loop() {
  PROFILE_LOOP_START();

  // Read button states
  bool startPressed = isStartPressed();
  bool stopPressed  = isStopPressed();
  bool chokePressed = isChokePressed();
  bool brakePressed = isBrakePressed();
  PROFILE_SECTION("Button reads");

  // Update hardware
  updateRelays(startPressed, stopPressed);
  updateServos(chokePressed, brakePressed);
  PROFILE_SECTION("Relay/Servo updates");

  // Safety check for startup
  if (startupInProgress || state.getTargetPercentage() == 0) {
    startPressed = false;
  }

  updateStartup(startPressed, stopPressed);
  PROFILE_SECTION("Startup update");
  
  handleWebUI();
  PROFILE_SECTION("WebUI handling");

  // Update managers
  profileManager.update();
  profileManager.checkModeSwitch(stopPressed);
  PROFILE_SECTION("Profile management");

  loraManager.update();
  heartbeatManager.update();
  PROFILE_SECTION("LoRa & Heartbeat");

  // Display management
  handleDisplayUpdates(stopPressed);
  PROFILE_SECTION("Display updates");

  // Servo control in manual mode
  if (currentState == MANUAL_CONTROL) {
    // Only adjust gas servo if remote is connected or if stopping
    if (heartbeatManager.isRemoteConnected() || state.getTargetPercentage() == 0) { 
        gasServo.write(calculateTargetAngle());
    }
  }
  PROFILE_SECTION("Manual control servo");

  PROFILE_LOOP_END();
  delay(5);
}

void handleDisplayUpdates(bool stopPressed) {
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
    // Fix: recover display after stop is released
    if (wasStopFlashing) {
      display.update(state.getDisplayedPercentage());
      wasStopFlashing = false;
    }

    // Screen updates - Now happens in both modes
    if (state.needsDisplayUpdate()) {
        state.updateDisplayStep();
        int dispPct = state.getDisplayedPercentage();
        display.update(dispPct);
        
        // Send display percentage to remote via LoRa
        loraManager.sendDisplayPercentage(dispPct);
    }
  }
}
