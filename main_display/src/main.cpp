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

// Note: Mode state is now managed by ProfileManager

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
  upButton.onHold([&]() { state.increase(); }, 75); // Reduced to 75ms to match remote responsiveness

  downButton.onPress([&]() { state.decrease(); });
  downButton.onHold([&]() { state.decrease(); }, 75); // Reduced to 75ms to match remote responsiveness

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
  
  // Send initial remote settings to ensure remote has valid configuration
  delay(1000); // Give remote time to initialize
  loraManager.sendRemoteSettings();
  
  // 🔄 Send initial mode information to remote
  loraManager.sendModeUpdate(profileManager.getCurrentProfile());
  
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
  bool localStopPressed = isStopPressed();
  bool remoteStopRequested = loraManager.getStopMotorRequest();
  
  // Track local stop button state changes for proper stop handling
  static bool previousLocalStopPressed = false;
  bool localStopJustPressed = localStopPressed && !previousLocalStopPressed;
  bool localStopJustReleased = !localStopPressed && previousLocalStopPressed;

  // Handle start button
  if (startRequested) {
    state.start();
  }
  
  // Handle local stop button with new hold-to-stay-stopped logic
  if (localStopJustPressed) {
    // Local stop button was just pressed - enter stop state (no timeout)
    state.stop();
  } else if (localStopPressed) {
    // Local stop button is being held - maintain stop state
    state.shouldStayStopped(true);
  } else if (localStopJustReleased) {
    // Local stop button was just released - exit stop state
    state.shouldStayStopped(false);
  }
  
  // Handle remote stop request with timeout (old behavior)
  if (remoteStopRequested) {
    state.stopWithTimeout(stopCooldownDuration); // Remote stops get 5 second timeout
  }
  
  // Update previous state for next iteration
  previousLocalStopPressed = localStopPressed;

  bool chokePressed = isChokePressed();
  bool brakePressed = isBrakePressed();

  // Update hardware
  updateRelays(state.getState() == StateManager::State::RUNNING, 
               state.getState() == StateManager::State::STOPPED);
  updateServos(chokePressed, brakePressed);

  updateStartup(startRequested, localStopPressed || remoteStopRequested);  

  handleWebUI();

  // Update managers
  profileManager.update();
  profileManager.checkModeSwitch(localStopPressed || remoteStopRequested);

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
  static unsigned long lastModeUpdateTime = 0; // Track last mode update sent
    static unsigned long lastLoRaTransmissionTime = 0; // Track last LoRa transmission time
  static const unsigned long MODE_UPDATE_INTERVAL = 2000; // Send mode every 2 seconds when not running
  static const unsigned long LORA_TRANSMISSION_THROTTLE = 250; // Minimum 250ms between LoRa transmissions
  
  // New: Delayed sending mechanism to reduce LoRa traffic during rapid changes
  static int lastStableDisplayPct = -1; // Track last stable percentage value
  static unsigned long lastPercentageChangeTime = 0; // When percentage last changed
  static const unsigned long PERCENTAGE_STABILIZATION_DELAY = 300; // Wait 300ms after last change before sending

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

  // Track percentage changes for stabilization delay
  if (dispPct != lastStableDisplayPct) {
      lastStableDisplayPct = dispPct;
      lastPercentageChangeTime = millis();
      Serial.printf("[Main Loop] Percentage changed to %d%%, starting stabilization timer\n", dispPct);
  }

  // Handle LoRa transmission with stabilization delay
  bool shouldSendNow = false;
  bool percentageHasStabilized = (millis() - lastPercentageChangeTime >= PERCENTAGE_STABILIZATION_DELAY);
  
  // Check if we should send: percentage has stabilized, it's different from last sent, and throttle period has passed
  if (dispPct != lastSentDisplayPct && percentageHasStabilized && 
      (millis() - lastLoRaTransmissionTime >= LORA_TRANSMISSION_THROTTLE)) {
      shouldSendNow = true;
  }
  
  if (shouldSendNow) {
      Serial.printf("[Main Loop] Percentage stabilized at %d%%, syncing to remote\n", dispPct);
      loraManager.sendDisplayPercentage(dispPct);
      lastSentDisplayPct = dispPct;
      lastLoRaTransmissionTime = millis();
  } else if (dispPct != lastSentDisplayPct && !percentageHasStabilized) {
      // If we're waiting for stabilization, log it for debugging
      unsigned long timeRemaining = PERCENTAGE_STABILIZATION_DELAY - (millis() - lastPercentageChangeTime);
      Serial.printf("[Main Loop] Waiting for stabilization: %d%% (remaining: %lu ms)\n", 
                    dispPct, timeRemaining);
  }

  // Send periodic mode updates when motor is not running to keep remote in sync
  bool motorRunning = (state.getState() == StateManager::State::RUNNING);
  if (!motorRunning && isConnected) {
      unsigned long currentTime = millis();
      if (currentTime - lastModeUpdateTime >= MODE_UPDATE_INTERVAL) {
          Serial.printf("[Main Loop] Sending periodic mode update: %s (motor not running)\n", currentMode);
          loraManager.sendModeUpdate(static_cast<uint8_t>(profileManager.getCurrentProfile()));
          lastModeUpdateTime = currentTime;
      }
  }

  wasStopped = isStopped;
  lastModeActive = modeActive;
}
