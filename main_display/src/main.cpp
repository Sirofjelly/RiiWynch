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
const unsigned long REMOTE_HEARTBEAT_TIMEOUT = 2000;
bool remoteConnected = false;

// --- FreeRTOS Task Handles ---
TaskHandle_t heartbeatMonitorTaskHandle = NULL;

// --- Mutexes for thread-safe access ---
SemaphoreHandle_t heartbeatMutex = NULL;
SemaphoreHandle_t loraMutex = NULL;

// --- LoRa Sync State ---
int lastSentDisplayPct = -1;
bool waitingForAck = false;
unsigned long lastLoraSendTime = 0;
const unsigned long LORA_RESEND_INTERVAL = 200; // ms

// --- LoRa ACK helper ---
void sendLoraAck(int pct) {
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
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
        
        xSemaphoreGive(loraMutex);
    }
}

// --- Heartbeat Monitoring Task Function ---
void heartbeatMonitorTask(void *parameter) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t checkPeriod = pdMS_TO_TICKS(100); // Check every 100ms for responsiveness
  
  for (;;) {
    // Wait for the next check time
    vTaskDelayUntil(&lastWakeTime, checkPeriod);
    
    // Take mutex before accessing shared heartbeat variables
    if (xSemaphoreTake(heartbeatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      // Check for heartbeat timeout
      if (remoteConnected && (millis() - lastRemoteHeartbeatTime > REMOTE_HEARTBEAT_TIMEOUT)) {
        Serial.println("[HBT Monitor] Remote connection lost! Initiating STOP sequence.");
        
        // Key safety action - stop the system immediately
        state.setTargetPercentage(0);
        
        // Update display to show connection lost
        display.updateText("NO REMOTE");
        
        // Update connection status
        remoteConnected = false;
        
        Serial.println("[HBT Monitor] Emergency stop executed");
      }
      
      // Release mutex
      xSemaphoreGive(heartbeatMutex);
    }
  }
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
