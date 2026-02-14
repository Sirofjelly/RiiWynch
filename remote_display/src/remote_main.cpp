#include <Arduino.h>
#include <Wire.h>
#include <FreeRTOS.h>
#include <atomic>
#include "LoRaManager_remote.h"
#include "Button.h"
#include "DisplayManager_remote.h"
#include "StateManager_remote.h"
#include "Settings_remote.h"
#include "SnakeGame.h"

// ─────────────────────────────────────────
//         BASIC FORWARD DECLARATIONS
// ─────────────────────────────────────────
void incPct(int step);
void decPct(int step);
uint16_t readBattery();
void heartbeatTask(void *parameter);

// Button callback wrappers
void incPctSingle();
void decPctSingle();

// LoRa callbacks
void onLoraSettingsReceived();
void onLoraRemoteSettingsReceived();
void onLoraStopMotor();
void onLoraModeUpdate(uint8_t idx);

// Game Globals
SnakeGame snakeGame;
unsigned long lastGameTick = 0;
const unsigned long GAME_TICK_MS = 200;
bool gamePaused = false;
bool isNewHighScore = false;

// ─────────────────────────────────────────
//            CONFIGURATION CONSTANTS
// ─────────────────────────────────────────
namespace Config {
    static const unsigned long DEBOUNCE_MS = 20;
    static const unsigned long REPEAT_MS = 200;
    static const unsigned long MENU_TIMEOUT_MS = 1500;
    static const unsigned long START_UPDATE_MS = 50;  // Reduced from 500ms to 50ms for responsive display
    static const unsigned long TRIPLE_TAP_WINDOW = 1500;
    static const int PERCENTAGE_STEP = 5;
    static const int SMOOTH_UPDATE_MS = 50;
    static const int SMOOTH_STEP = 5;
    
    // New constants for dead man's switch logic
    static const unsigned long ARMING_DURATION_MS = 1000;
    static const unsigned long KEEPALIVE_INTERVAL_MS = 500;
    // NO_BUTTON_TIMEOUT_MS moved to global variable remoteStopDelayMs
    static const unsigned long HEARTBEAT_INTERVAL_MS = 500;
    // Dual-button gesture timings
    static const unsigned long DUAL_PRESS_WINDOW_MS = 150;    // Time to wait for 2nd button
    static const unsigned long DELAY_HOLD_MS      = 600;    // Hold time to toggle delay
}

// ─── Managers and Components ───
DisplayManager_remote displayManager;
LoRaManager_remote loraManager;
StateManager_remote stateManager;
Button upButton(7);
Button downButton(4);

// ─── FreeRTOS Task Handles ───
TaskHandle_t heartbeatTaskHandle = NULL;

// ─── Pins ───
#define VEXT      21
#define VBAT      1   // Battery voltage pin (GPIO1) - voltage divider output
#define ADC_CTRL  37  // ADC control pin - must be LOW to enable battery reading

// ─── Application State ───
float currentRSSI = 0.0f;
unsigned long lastDisplayUpdate = 0;
unsigned long armingStartTime = 0;
unsigned long noButtonPressStartTime = 0;
unsigned long lastKeepaliveTime = 0;
bool stopDelayActive = false; // Flag for delayed stop functionality

// 🔄 Mode tracking - use atomic to protect against race conditions
// between LoRa callback (write) and main loop (read)
static std::atomic<uint8_t> currentModeIdx{0};
static const char* modeNames[4] = {"SURF", "SKIM", "SMOOTH", "MANUAL"};

// ─── Remote Settings ───
extern unsigned long remoteStopDelayMs; // Configurable stop delay from Settings_remote

// ─── Dual-button gesture FSM ───
enum class DualPressState { IDLE_WAIT, FIRST_DOWN, BOTH_DOWN };
static DualPressState dualState = DualPressState::IDLE_WAIT;
static unsigned long firstButtonTime = 0;   // Timestamp of first detected button
static unsigned long bothButtonsTime  = 0;   // Timestamp when both buttons are held
static bool dualActionProcessed = false;    // Prevent multiple toggles per gesture

// ─────────────────────────────────────────
//           LORA CALLBACKS
// ─────────────────────────────────────────

void onLoraDisplayUpdate(int percentage, float rssi) {
    currentRSSI = rssi; // Keep this as fallback, but real-time RSSI is now used for display
    Serial.printf("[LORA CB] DSP: %d%% (current state: %d, targetPct: %d, shownPct: %d)\n", 
                 percentage, (int)stateManager.getState(), stateManager.getTargetPercentage(), stateManager.getShownPercentage());

    if (stateManager.getState() != StateManager_remote::State::MENU) {
        stateManager.setTargetPercentage(percentage);
        Serial.printf("[Remote] Updated display to %d%% from main\n", percentage);
    } else {
        Serial.println("[Remote] Ignoring DSP update value - in MENU mode, but ACK sent.");
    }
}

void onLoraAckForValue(int percentage) {
    Serial.printf("[Remote] Received ACK from Main for VAL %d%%\n", percentage);
}

void onLoraSettingsReceived() {
    Serial.println("[Remote] LoRa settings received from main - restarting LoRa module...");
    if (loraManager.restart()) {
        Serial.println("[Remote] LoRa module restarted successfully with new settings");
    } else {
        Serial.println("[Remote] Failed to restart LoRa module with new settings");
    }
}

void onLoraRemoteSettingsReceived() {
    Serial.println("[Remote] Remote settings received and applied from main");
}

void onLoraStopMotor() {
    Serial.println("[Remote] STOP_MOTOR received – switching to IDLE");
    stateManager.switchToIdle();
    stopDelayActive = false; // Reset delay flag when ride ends
}

// 🔄 MODE_UPDATE callback - uses atomic store for thread safety
void onLoraModeUpdate(uint8_t idx) {
    Serial.printf("[Remote] onLoraModeUpdate called with idx=%d\n", idx);
    if (idx < 4) {
        currentModeIdx.store(idx, std::memory_order_release);
        Serial.printf("[Remote] Mode updated to %s (%d)\n", modeNames[idx], idx);
    } else {
        Serial.printf("[Remote] Invalid mode index received: %d\n", idx);
    }
}

// ─────────────────────────────────────────
//           BUTTON HANDLING
// ─────────────────────────────────────────

void handleMenuNavigation(bool isUp) {
    if (stateManager.getState() != StateManager_remote::State::MENU) return;

    // This function now only handles single presses for menu navigation.
    if (isUp) {
        incPctSingle();
    } else {
        decPctSingle();
    }
}

void enterMenuOnTriplePress() {
    if (stateManager.getState() != StateManager_remote::State::IDLE) return;

    static unsigned long lastTapTime = 0;
    static int tapCount = 0;

    if (millis() - lastTapTime > Config::TRIPLE_TAP_WINDOW) {
        tapCount = 0;
    }

    tapCount++;
    lastTapTime = millis();

    if (tapCount >= 3) {
        Serial.println("TRIPLE PRESS → MENU");
        stateManager.switchToMenu();
        tapCount = 0;
    }
}

void handleUpButtonPress() {
    if (stateManager.getState() == StateManager_remote::State::IDLE) {
        enterMenuOnTriplePress();
    } else if (stateManager.getState() == StateManager_remote::State::MENU) {
        handleMenuNavigation(true);
            } else if (stateManager.getState() == StateManager_remote::State::GAME) {
                snakeGame.turnLeft();
            } else if (stateManager.getState() == StateManager_remote::State::GAME_OVER) {
                stateManager.switchToIdle();
                lastDisplayUpdate = 0; // Force immediate update
            }}

void handleDownButtonPress() {
    if (stateManager.getState() == StateManager_remote::State::IDLE) {
        enterMenuOnTriplePress();
    } else if (stateManager.getState() == StateManager_remote::State::MENU) {
        handleMenuNavigation(false);
    } else if (stateManager.getState() == StateManager_remote::State::GAME) {
        snakeGame.turnRight();
    } else if (stateManager.getState() == StateManager_remote::State::GAME_OVER) {
        stateManager.switchToIdle();
        lastDisplayUpdate = 0; // Force immediate update
    }
}

void incPct(int step) {
    if (stateManager.getState() == StateManager_remote::State::MENU) {
        stateManager.increasePercentage(step);
        Serial.printf("INC (%d) → %d\n", step, stateManager.getTargetPercentage());
    }
}

void decPct(int step) {
    if (stateManager.getState() == StateManager_remote::State::MENU) {
        stateManager.decreasePercentage(step);
        Serial.printf("DEC (%d) → %d\n", step, stateManager.getTargetPercentage());
    }
}

// Wrapper functions for callbacks
void incPctSingle() { incPct(1); }
void decPctSingle() { decPct(1); }

// ─────────────────────────────────────────
//            HEARTBEAT TASK
// ─────────────────────────────────────────
void heartbeatTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t heartbeatPeriod = pdMS_TO_TICKS(Config::HEARTBEAT_INTERVAL_MS);
    
    for (;;) {
        vTaskDelayUntil(&lastWakeTime, heartbeatPeriod);
        loraManager.sendHeartbeat();
    }
}

// ─────────────────────────────────────────
//              DISPLAY LOGIC
// ─────────────────────────────────────────
void drawForState() {
    // Atomic load of mode index for thread safety with LoRa callback
    uint8_t modeIdx = currentModeIdx.load(std::memory_order_acquire);

    switch(stateManager.getState()) {
        case StateManager_remote::State::IDLE:
        case StateManager_remote::State::ARMING:
        case StateManager_remote::State::CRUISING:
             displayManager.drawStartScreen(stateManager.getShownPercentage(), loraManager.getCurrentRSSI(), readBattery(), stateManager.getState(), modeNames[modeIdx], stopDelayActive, remoteStopDelayMs);
            break;
        case StateManager_remote::State::MENU:
            displayManager.drawMenuScreen(stateManager.getShownPercentage());
            break;
        case StateManager_remote::State::GAME:
            displayManager.drawGameScreen(snakeGame, snakeHighScore);
            break;
        case StateManager_remote::State::GAME_OVER:
            displayManager.drawGameOverScreen(snakeGame.getScore(), snakeHighScore, isNewHighScore);
            break;
    }
}
uint16_t readBattery() {
    // Heltec V3 voltage divider: VBAT - 390k - GPIO1 - 100k - GND
    // Divider ratio: (390k + 100k) / 100k = 4.9
    // GPIO37 (ADC_CTRL) must be LOW to enable reading (HIGH for V3.2 boards)
    const float VREF = 3.3;        // ADC reference voltage
    const int MAX = 4095;          // 12-bit ADC resolution
    const float DIVIDER = 4.9f;  // Voltage divider ratio ((390k + 100k) / 100k)

    // Enable battery voltage divider (LOW for V3/V3.1, use HIGH only on boards that require it)
    digitalWrite(ADC_CTRL, LOW);
    delayMicroseconds(100);  // Let voltage stabilize

    // Average multiple readings to reduce ADC noise
    const int NUM_SAMPLES = 16;
    uint32_t rawSum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        rawSum += analogRead(VBAT);
        delayMicroseconds(50);
    }
    int raw = rawSum / NUM_SAMPLES;

    // Calculate actual voltage: ADC_voltage * divider_ratio
    float voltage = (raw * VREF / MAX) * DIVIDER;

    // Return voltage in millivolts
    return (uint16_t)(voltage * 1000);
}

// ─────────────────────────────────────────
//                 SETUP
// ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("Starting Setup of Remote...");

    pinMode(VEXT, OUTPUT); digitalWrite(VEXT, LOW);
    pinMode(ADC_CTRL, OUTPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(VBAT, ADC_11db);  // Set proper ADC range for battery voltage reading

    // Load global LoRa settings
    loadGlobalLoRaSettings();
    loadSnakeHighScore();

    displayManager.begin();
    drawForState();

    if (!loraManager.begin()) {
        Serial.println("LoRa Manager init failed!");
    }
    
    loraManager.onDisplayUpdate(onLoraDisplayUpdate);
    loraManager.onAckForValue(onLoraAckForValue);
    loraManager.onLoRaSettingsReceived(onLoraSettingsReceived);
    loraManager.onRemoteSettingsReceived(onLoraRemoteSettingsReceived);
    loraManager.onStopMotor(onLoraStopMotor);
    loraManager.onModeUpdate(onLoraModeUpdate);

    // --- Button Callbacks ---
    upButton.onPress(handleUpButtonPress);
    upButton.onHold(incPctSingle, 75);
    
    downButton.onPress(handleDownButtonPress);
    downButton.onHold(decPctSingle, 75);
    
    // The heartbeat task is no longer needed as KEEPALIVE serves a more specific purpose
    xTaskCreatePinnedToCore(heartbeatTask, "HeartbeatTask", 2048, NULL, 1, &heartbeatTaskHandle, 0);
    
    Serial.println("Setup complete.");
}

// ─────────────────────────────────────────
//                 MAIN LOOP
// ─────────────────────────────────────────
void loop() {
    loraManager.update();
    upButton.update();
    downButton.update();

    // Update real-time RSSI monitoring
    loraManager.updateRealTimeRSSI();

    bool anyButtonPressed = upButton.isPressed() || downButton.isPressed();

    switch (stateManager.getState()) {
        case StateManager_remote::State::IDLE: {
            /* ───────── Dual-button FSM for reliable delay toggle ───────── */
            switch (dualState) {
                case DualPressState::IDLE_WAIT:
                    if (anyButtonPressed) {
                        dualState = DualPressState::FIRST_DOWN;
                        firstButtonTime = millis();
                    }
                    break;

                case DualPressState::FIRST_DOWN:
                    if (upButton.isPressed() && downButton.isPressed()) {
                        dualState = DualPressState::BOTH_DOWN;
                        bothButtonsTime = millis();
                        dualActionProcessed = false;
                    } else if (millis() - firstButtonTime > Config::DUAL_PRESS_WINDOW_MS) {
                        // Treated as single-button press → enter ARMING
                        dualState = DualPressState::IDLE_WAIT;
                        stateManager.switchToArming();
                        armingStartTime = millis();
                        Serial.println("IDLE → ARMING (single button confirmed)");
                    } else if (!anyButtonPressed) {
                        // Tap & release within window – likely triple-press gesture
                        dualState = DualPressState::IDLE_WAIT;
                    }
                    break;

                case DualPressState::BOTH_DOWN:
                     // 1. Check for Game Entry (Hold Both > 2s)
                    if (millis() - bothButtonsTime > 2000) {
                        Serial.println("Entering Snake Game!");
                        
                        // Revert the delay toggle if it happened at 600ms
                        if (dualActionProcessed) {
                            stopDelayActive = !stopDelayActive;
                            Serial.println("[Remote] Delay toggle reverted due to Game Entry");
                        }
                        
                        stateManager.switchToGame();
                        snakeGame.reset();
                        gamePaused = false;
                        
                        // Reset FSM
                        dualState = DualPressState::IDLE_WAIT;
                        dualActionProcessed = false;
                        return;
                    }

                    if (!(upButton.isPressed() && downButton.isPressed())) {
                        // One or both buttons released → reset FSM
                        dualState = DualPressState::IDLE_WAIT;
                    } else {
                        // 2. Normal Delay Toggle (Hold Both > 600ms)
                        if (!dualActionProcessed && millis() - bothButtonsTime >= Config::DELAY_HOLD_MS) {
                            stopDelayActive = !stopDelayActive;
                            dualActionProcessed = true;
                            if (stopDelayActive) Serial.println("[Remote] Delay activated for next ride");
                            else Serial.println("[Remote] Delay deactivated");
                        }
                    }
                    break;
            }
            break;
        }

        case StateManager_remote::State::ARMING: {
            if (!anyButtonPressed) {
                stateManager.switchToIdle();
                Serial.println("ARMING → IDLE");
                break;
            }
            
            if (millis() - armingStartTime >= Config::ARMING_DURATION_MS) {
                Serial.println("ARMING → CRUISING (START_MOTOR sent)");
                loraManager.sendStartMotor();
                stateManager.switchToCruising();
                lastKeepaliveTime = millis(); // Send first keepalive immediately
                noButtonPressStartTime = 0; // Reset this timer
            }
            break;
        }

        case StateManager_remote::State::CRUISING: {
            if (anyButtonPressed) {
                noButtonPressStartTime = 0; // Reset timer because a button is pressed

                if (millis() - lastKeepaliveTime >= Config::KEEPALIVE_INTERVAL_MS) {
                    loraManager.sendKeepalive();
                    lastKeepaliveTime = millis();
                    Serial.println("KEEPALIVE sent");
                }
            } else { // No buttons are pressed
                if (stopDelayActive) {
                    // Use delay logic when delay is active
                    if (noButtonPressStartTime == 0) {
                        // Start the timer
                        noButtonPressStartTime = millis();
                        Serial.println("No button press timer started (with delay)...");
                    } else if (millis() - noButtonPressStartTime >= remoteStopDelayMs) {
                        Serial.println("CRUISING → IDLE (STOP_MOTOR sent after delay)");
                        loraManager.sendStopMotor();
                        stateManager.switchToIdle();
                        stopDelayActive = false; // Reset delay flag when ride ends
                    }
                } else {
                    // Immediate stop when delay is not active
                    Serial.println("CRUISING → IDLE (STOP_MOTOR sent immediately)");
                    loraManager.sendStopMotor();
                    stateManager.switchToIdle();
                }
            }
            break;
        }

        case StateManager_remote::State::MENU: {
            // In menu, button actions are handled by callbacks
            if (anyButtonPressed) {
                stateManager.resetMenuActivityTimer();
            }
            
            if (stateManager.isMenuTimedOut(Config::MENU_TIMEOUT_MS)) {
                Serial.printf("[Remote] Menu timeout - sending VAL message with %d%% to main\n", stateManager.getTargetPercentage());
                loraManager.sendValue(stateManager.getTargetPercentage());
                stateManager.switchToIdle(); // Return to IDLE
                break;
            }
            break;
        }

        case StateManager_remote::State::GAME: {
             // Pause (Both buttons 600ms)
             static unsigned long gameBothPressStart = 0;
             if (upButton.isPressed() && downButton.isPressed()) {
                 if (gameBothPressStart == 0) gameBothPressStart = millis();
                 else if (millis() - gameBothPressStart > 600) {
                     gamePaused = !gamePaused;
                     gameBothPressStart = 0; // Reset to avoid rapid toggling
                 }
             } else {
                 gameBothPressStart = 0;
             }

             if (!gamePaused && millis() - lastGameTick > snakeGame.getCurrentSpeed()) {
                 snakeGame.update();
                 lastGameTick = millis();
                 if (snakeGame.isGameOver()) {
                     uint16_t score = snakeGame.getScore();
                     isNewHighScore = (score > snakeHighScore);
                     if (isNewHighScore) {
                         snakeHighScore = score;
                         saveSnakeHighScore();
                     }
                     stateManager.switchToGameOver();
                 }
             }
             break;
        }

        case StateManager_remote::State::GAME_OVER: {
             // Button handling is done via callbacks
             break;
        }
    }

    stateManager.updateShownPercentage(Config::SMOOTH_STEP, Config::SMOOTH_UPDATE_MS);

    // Centralized display update
    if (millis() - lastDisplayUpdate >= Config::START_UPDATE_MS) {
        drawForState();
        lastDisplayUpdate = millis();
    }

    delay(5);
}
