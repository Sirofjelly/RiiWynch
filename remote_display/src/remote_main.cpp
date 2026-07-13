#include <Arduino.h>
#include <Wire.h>
#include <FreeRTOS.h>
#include "LoRaManager_remote.h"
#include "Button.h"
#include "DisplayManager_remote.h"
#include "StateManager_remote.h"
#include "Settings_remote.h"

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

// ─────────────────────────────────────────
//            CONFIGURATION CONSTANTS
// ─────────────────────────────────────────
namespace Config {
    static const unsigned long DEBOUNCE_MS = 20;
    static const unsigned long REPEAT_MS = 200;
    static const unsigned long MENU_TIMEOUT_MS = 1500;
    static const unsigned long START_UPDATE_MS = 500;
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
#define VEXT     21
#define VBAT     1  // Battery voltage pin for Heltec V3 is GPIO1 (voltage divider output)
#define ADC_CTRL 37 // ADC control pin (must be LOW to enable the onboard divider)

// ─── Application State ───
float currentRSSI = 0.0f;
unsigned long lastDisplayUpdate = 0;
unsigned long armingStartTime = 0;
unsigned long noButtonPressStartTime = 0;
unsigned long lastKeepaliveTime = 0;
bool stopDelayActive = false; // Flag for delayed stop functionality

// 🔄 Mode tracking
static uint8_t currentModeIdx = 0;
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
    currentRSSI = rssi;
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

// 🔄 MODE_UPDATE callback
void onLoraModeUpdate(uint8_t idx) {
    Serial.printf("[Remote] onLoraModeUpdate called with idx=%d\n", idx);
    if (idx < 4) {
        currentModeIdx = idx;
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
    }
}

void handleDownButtonPress() {
    if (stateManager.getState() == StateManager_remote::State::IDLE) {
        enterMenuOnTriplePress();
    } else if (stateManager.getState() == StateManager_remote::State::MENU) {
        handleMenuNavigation(false);
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
    switch(stateManager.getState()) {
        case StateManager_remote::State::IDLE:
        case StateManager_remote::State::ARMING:
        case StateManager_remote::State::CRUISING:
             displayManager.drawStartScreen(stateManager.getShownPercentage(), currentRSSI, readBattery(), stateManager.getState(), modeNames[currentModeIdx], stopDelayActive, remoteStopDelayMs);
            break;
        case StateManager_remote::State::MENU:
            displayManager.drawMenuScreen(stateManager.getShownPercentage());
            break;
    }
}
uint16_t readBattery() {
    const float VREF = 3.3;        // Reference voltage for ADC
    const int MAX = 4095;          // 12-bit ADC resolution
    const float DIV = 4.9;          // Voltage divider ratio for Heltec V3 (390k + 100k -> 4.9)
    const float MIN_VOLTAGE = 2.5; // Minimum discharge voltage
    const float MAX_VOLTAGE = 4.2; // Maximum charge voltage
    
    // On Heltec V3 boards the divider is controlled by ADC_CTRL.
    // Ensure ADC_CTRL is LOW to connect the divider, then sample VBAT (GPIO1).
    digitalWrite(ADC_CTRL, LOW);
    delayMicroseconds(100);
    int raw = analogRead(VBAT);

    // Calculate actual voltage
    float voltage = (raw * VREF / MAX) * DIV;

    // Calculate percentage based on voltage range
    float percentage = (voltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE) * 100.0;
    percentage = constrain(percentage, 0.0, 100.0);

    // Debug output to help diagnose wiring/pin issues
    Serial.printf("[BAT] raw=%d, volt=%.3fV, pct=%.1f%%\n", raw, voltage, percentage);

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
    analogReadResolution(12);
    pinMode(ADC_CTRL, OUTPUT);
    digitalWrite(ADC_CTRL, LOW); // Enable voltage divider by default
    // Ensure ADC attenuation is set so the ADC can read up to battery voltage
    // Use ADC_11db (~3.6V - 3.9V range) on ESP32 for LiPo measurement
    analogSetPinAttenuation(VBAT, ADC_11db);

    // Load global LoRa settings
    loadGlobalLoRaSettings();

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
                    if (!(upButton.isPressed() && downButton.isPressed())) {
                        // One or both buttons released → reset FSM
                        dualState = DualPressState::IDLE_WAIT;
                    } else {
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
                lastKeepaliveTime = millis();
                noButtonPressStartTime = 0;
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
    }

    stateManager.updateShownPercentage(Config::SMOOTH_STEP, Config::SMOOTH_UPDATE_MS);

    // Centralized display update
    if (millis() - lastDisplayUpdate >= Config::START_UPDATE_MS) {
        drawForState();
        lastDisplayUpdate = millis();
    }

    delay(5);
}