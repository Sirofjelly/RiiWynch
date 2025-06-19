#include <WiFi.h>
#include <WebServer.h>
#include "Buttons.h"
#include "Relays.h"
#include "Servos.h"
#include "StartupManager.h"
#include "WebUI.h"
#include "DisplayManager.h"
#include "StateManager.h"
#include "Settings.h"  // 🆕 Include for loadSettings()
#include "LoRaManager.h"
#include "HeartbeatManager.h"
#include "ProfileManager.h"
#include "TaskManager.h"
#include <RadioLib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "Button.h"

// Component instances
DisplayManager display;
Button upButton(7);
Button downButton(40);
StateManager state; // Make state a regular instance

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

LoRaManager& getGlobalLoRaManager() {
  return loraManager;
}

ProfileManager& getGlobalProfileManager() {
  return profileManager;
}

// Function prototype for handleDisplayUpdates
void handleDisplayUpdates(bool isStopped);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Setup.");
  
  // Initialize new buttons with callbacks
  upButton.onPress([&]() { state.increase(); });
  upButton.onHold([&]() { state.increase(); }, 100); // 100ms repeat interval

  downButton.onPress([&]() { state.decrease(); });
  downButton.onHold([&]() { state.decrease(); }, 100);

  // Initialize existing subsystems
  setupButtons();
  setupRelays();
  setupServos();
  setupStartup();
  setupWebUI();
  display.begin();
  display.update(state.getDisplayedPercentage(), loraManager.getRSSI(), profileManager.getCurrentModeName(), heartbeatManager.isRemoteConnected());

  // Initialize managers
  Serial.println("Initializing managers...");
  
  // Load global settings (LoRa settings)
  loadGlobalSettings();
  loadStats();
  
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
  // Update new buttons
  upButton.update();
  downButton.update();

  // Update state machine
  state.update();

  // Cache button and remote request states for this loop iteration
  bool startRequested = isStartPressed() || loraManager.getStartMotorRequest();
  bool stopRequested = isStopPressed() || loraManager.getStopMotorRequest();

  // Read button states
  if (startRequested) {
    state.start();
  }
  if (stopRequested) {
    state.stop();
  }

  bool chokePressed = isChokePressed();
  bool brakePressed = isBrakePressed();

  // Update hardware
  updateRelays(state.getState() == StateManager::State::RUNNING, 
               state.getState() == StateManager::State::STOPPED);
  updateServos(chokePressed, brakePressed);

  updateStartup(startRequested, stopRequested);  

  handleWebUI();

  // Update managers
  profileManager.update();
  profileManager.checkModeSwitch(stopRequested);

  loraManager.update();
  heartbeatManager.update();

  // Display management
  handleDisplayUpdates(state.getState() == StateManager::State::STOPPED);

  // Servo control in manual mode
  if (currentState == MANUAL_CONTROL) {
    // Only adjust gas servo if remote is connected or if stopping
    if (heartbeatManager.isRemoteConnected() || state.getTargetPercentage() == 0) { 
        gasServo.write(calculateTargetAngle());
    }
  }
  delay(5);
}

void handleDisplayUpdates(bool isStopped) {
  static int lastSentDisplayPct = -1; // Track last sent percentage to avoid duplicates
  static bool wasStopped = false;     // Track previous stopped state
  static bool lastModeActive = false; // Track mode display state

  // Check if mode display is currently active
  bool modeActive = display.isModeDisplayActive();
  
  int dispPct = state.getDisplayedPercentage();
  const char* currentMode = profileManager.getCurrentModeName();
  bool isConnected = heartbeatManager.isRemoteConnected();

  if (isStopped) {
      // Draw STOP screen with mode and connection info
      display.drawStopScreen(dispPct, loraManager.getRSSI(), currentMode, isConnected);
  } else {
      // If the mode screen just finished, draw the percentage screen once
      if (!modeActive && lastModeActive && !isStopped) {
          display.update(dispPct, loraManager.getRSSI(), currentMode, isConnected);
      }
      
      // Regular percentage display logic
      if (state.needsDisplayUpdate() || wasStopped) {
          state.updateDisplayStep();
          display.update(dispPct, loraManager.getRSSI(), currentMode, isConnected);
      }
  }

  // Always sync percentage to remote when it changes
  if (dispPct != lastSentDisplayPct) {
      Serial.printf("[Main Loop] Display updated to %d%%, syncing to remote\n", dispPct);
      loraManager.sendDisplayPercentage(dispPct);
      lastSentDisplayPct = dispPct;
  }

  wasStopped = isStopped;
  lastModeActive = modeActive;
}
