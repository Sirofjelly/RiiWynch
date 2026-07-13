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

// Mutex for protecting shared display update state
static SemaphoreHandle_t displayUpdateMutex = NULL;

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

  // Initialize mutex for display update state protection
  displayUpdateMutex = xSemaphoreCreateMutex();
  if (displayUpdateMutex == NULL) {
    Serial.println("Failed to create display update mutex!");
  }
  
  // Initialize new buttons with callbacks
  upButton.onPress([&]() { 
    if (!profileManager.isInModeSwitchState()) {
      state.increase(); 
    }
  });
  upButton.onHold([&]() { 
    if (!profileManager.isInModeSwitchState()) {
      state.increase(); 
    }
  }, 75); // Reduced to 75ms to match remote responsiveness

  downButton.onPress([&]() { 
    if (!profileManager.isInModeSwitchState()) {
      state.decrease(); 
    }
  });
  downButton.onHold([&]() { 
    if (!profileManager.isInModeSwitchState()) {
      state.decrease(); 
    }
  }, 75); // Reduced to 75ms to match remote responsiveness

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

  // Update real-time RSSI monitoring
  loraManager.updateRealTimeRSSI();

  // Cache button and remote request states for this loop iteration
  bool localStartPressed = isStartPressed();
  bool remoteStartRequested = loraManager.getStartMotorRequest();
  bool startRequested = localStartPressed || remoteStartRequested;
  bool localStopPressed = isStopPressed();
  bool remoteStopRequested = loraManager.getStopMotorRequest();
  
  // Track local stop button state changes for proper stop handling
  static bool previousLocalStopPressed = false;
  bool localStopJustPressed = localStopPressed && !previousLocalStopPressed;
  bool localStopJustReleased = !localStopPressed && previousLocalStopPressed;

  // Handle start button / remote start acceptance
  StateManager::State stateBeforeStart = state.getState();
  bool canAcceptRemoteStart = remoteStartRequested &&
                              (stateBeforeStart == StateManager::State::IDLE ||
                               stateBeforeStart == StateManager::State::RUNNING) &&
                              !localStopPressed &&
                              !isStopRelayInCooldown();
  if (startRequested) {
    state.start();
    if (canAcceptRemoteStart && state.getState() == StateManager::State::RUNNING) {
      loraManager.sendStartAccepted();
    }
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

  bool disconnectedLocalOverrideActive = !heartbeatManager.isRemoteConnected() &&
                                         state.getState() == StateManager::State::RUNNING;
  static bool lastDisconnectedLocalOverrideActive = false;
  if (disconnectedLocalOverrideActive != lastDisconnectedLocalOverrideActive) {
    Serial.printf("[Main] Local disconnect speed override %s\n",
                  disconnectedLocalOverrideActive ? "active" : "inactive");
    lastDisconnectedLocalOverrideActive = disconnectedLocalOverrideActive;
  }

  updateStartup(startRequested, localStopPressed || remoteStopRequested, disconnectedLocalOverrideActive);  

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
    // Allow local speed control even when remote is disconnected while the system is running.
    if (heartbeatManager.isRemoteConnected() ||
        state.getTargetPercentage() == 0 ||
        disconnectedLocalOverrideActive) { 
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
  static unsigned long lastDisplayRefreshTime = 0; // Track last screen refresh for RSSI updates
  static const unsigned long MODE_UPDATE_INTERVAL = 2000; // Send mode every 2 seconds when not running
  static const unsigned long LORA_TRANSMISSION_THROTTLE = 250; // Minimum 250ms between LoRa transmissions
  static const unsigned long DISPLAY_REFRESH_INTERVAL = 250; // Refresh screen even if nothing else changed

  // Protected by displayUpdateMutex: Delayed sending mechanism to reduce LoRa traffic during rapid changes
  static int lastStableDisplayPct = -1; // Track last stable percentage value
  static unsigned long lastPercentageChangeTime = 0; // When percentage last changed
  static const unsigned long PERCENTAGE_STABILIZATION_DELAY = 300; // Wait 300ms after last change before sending

  // Check if mode display is currently active
  bool modeActive = display.isModeDisplayActive();

  int dispPct = state.getDisplayedPercentage();
  const char* currentMode = profileManager.getCurrentModeName();
  float currentRssi = loraManager.getCurrentRSSI();
  bool hasFreshRssi = currentRssi > -900.0f;
  bool isConnected = heartbeatManager.isRemoteConnected() || hasFreshRssi;

  bool shouldRefreshDisplay = false;
  unsigned long currentTime = millis();
  if (currentTime - lastDisplayRefreshTime >= DISPLAY_REFRESH_INTERVAL) {
      shouldRefreshDisplay = true;
      lastDisplayRefreshTime = currentTime;
  }

  if (isStopped) {
      // Draw STOP screen with mode and connection info - using real-time RSSI
      display.drawStopScreen(dispPct, currentRssi, currentMode, isConnected);
  } else {
      // If the mode screen just finished, draw the percentage screen once
      if (!modeActive && lastModeActive && !isStopped) {
          display.update(dispPct, currentRssi, currentMode, isConnected);
      }

      // Regular percentage display logic
      if (state.needsDisplayUpdate() || wasStopped || shouldRefreshDisplay) {
          state.updateDisplayStep();
          display.update(dispPct, currentRssi, currentMode, isConnected);
      }
  }

  // === MUTEX-PROTECTED SECTION for percentage stabilization tracking ===
  // This prevents race conditions between main loop and LoRa/heartbeat callbacks
  bool shouldSendNow = false;
  int pctToSend = 0;

  if (displayUpdateMutex != NULL && xSemaphoreTake(displayUpdateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      unsigned long currentTime = millis();

      // Track percentage changes for stabilization delay
      if (dispPct != lastStableDisplayPct) {
          lastStableDisplayPct = dispPct;
          lastPercentageChangeTime = currentTime;
          Serial.printf("[Main Loop] Percentage changed to %d%%, starting stabilization timer\n", dispPct);
      }

      // Handle LoRa transmission with stabilization delay (rollover-safe comparison)
      unsigned long timeSinceChange = currentTime - lastPercentageChangeTime;
      bool percentageHasStabilized = (timeSinceChange >= PERCENTAGE_STABILIZATION_DELAY);

      unsigned long timeSinceLastTx = currentTime - lastLoRaTransmissionTime;

      // Check if we should send: percentage has stabilized, it's different from last sent, and throttle period has passed
      if (dispPct != lastSentDisplayPct && percentageHasStabilized &&
          (timeSinceLastTx >= LORA_TRANSMISSION_THROTTLE)) {
          shouldSendNow = true;
          pctToSend = dispPct;
          lastSentDisplayPct = dispPct;
          lastLoRaTransmissionTime = currentTime;
      } else if (dispPct != lastSentDisplayPct && !percentageHasStabilized) {
          // If we're waiting for stabilization, log it for debugging
          unsigned long timeRemaining = PERCENTAGE_STABILIZATION_DELAY - timeSinceChange;
          Serial.printf("[Main Loop] Waiting for stabilization: %d%% (remaining: %lu ms)\n",
                        dispPct, timeRemaining);
      }

      xSemaphoreGive(displayUpdateMutex);
  }
  // === END MUTEX-PROTECTED SECTION ===

  // Perform LoRa transmission outside of mutex to avoid holding lock during I/O
  if (shouldSendNow) {
      Serial.printf("[Main Loop] Percentage stabilized at %d%%, syncing to remote\n", pctToSend);
      loraManager.sendDisplayPercentage(pctToSend);
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
