#include <Arduino.h>
#include <Wire.h>
#include <FreeRTOS.h>
#include "LoRaManager_remote.h"
#include "Button.h"
#include "DisplayManager_remote.h"
#include "StateManager_remote.h"

// ─────────────────────────────────────────
//         BASIC FORWARD DECLARATIONS
// ─────────────────────────────────────────
void incPct();
void decPct();
uint16_t readBattery();
void heartbeatTask(void *parameter);

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
    static const unsigned long ARMING_DURATION_MS = 2000;
    static const unsigned long KEEPALIVE_INTERVAL_MS = 500;
    static const unsigned long NO_BUTTON_TIMEOUT_MS = 1000;
    static const unsigned long HEARTBEAT_INTERVAL_MS = 500;
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
#define ADC_CTRL 37
#define VEXT     21
#define VBAT     2 // Battery voltage pin for Heltec WiFi LoRa 32 V3

// ─── Application State ───
float currentRSSI = 0.0f;
unsigned long lastDisplayUpdate = 0;
unsigned long armingStartTime = 0;
unsigned long noButtonPressStartTime = 0;
unsigned long lastKeepaliveTime = 0;

// ─────────────────────────────────────────
//           LORA CALLBACKS
// ─────────────────────────────────────────

void onLoraDisplayUpdate(int percentage, float rssi) {
    currentRSSI = rssi;
    Serial.printf("[LORA CB] DSP: %d%% (current state: %d, targetPct: %d, shownPct: %d)\n", 
                 percentage, (int)stateManager.getState(), stateManager.getTargetPercentage(), stateManager.getShownPercentage());

    // If main display says percentage is 0, it has likely stopped.
    // Force remote back to IDLE as a safety measure.
    if (percentage == 0 && stateManager.getState() == StateManager_remote::State::CRUISING) {
        stateManager.switchToIdle();
    }

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

// ─────────────────────────────────────────
//           BUTTON HANDLING
// ─────────────────────────────────────────
void registerTripleTap() {
    static unsigned long tapTimes[3] = {0, 0, 0};
    
    tapTimes[0] = tapTimes[1];
    tapTimes[1] = tapTimes[2];
    tapTimes[2] = millis();

    if (tapTimes[0] > 0 && (tapTimes[2] - tapTimes[0] <= Config::TRIPLE_TAP_WINDOW)) {
        Serial.println("TRIPLE PRESS → MENU");
        tapTimes[0] = tapTimes[1] = tapTimes[2] = 0;
        stateManager.switchToMenu();
    }
}

void incPct() {
    if (stateManager.getState() == StateManager_remote::State::MENU) {
        stateManager.increasePercentage(Config::PERCENTAGE_STEP);
        Serial.printf("INC → %d\n", stateManager.getTargetPercentage());
    }
}

void decPct() {
    if (stateManager.getState() == StateManager_remote::State::MENU) {
        stateManager.decreasePercentage(Config::PERCENTAGE_STEP);
        Serial.printf("DEC → %d\n", stateManager.getTargetPercentage());
    }
}

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
             displayManager.drawStartScreen(stateManager.getShownPercentage(), currentRSSI, readBattery(), stateManager.getState());
            break;
        case StateManager_remote::State::MENU:
            displayManager.drawMenuScreen(stateManager.getShownPercentage());
            break;
    }
}

uint16_t readBattery() {
    const float VREF = 3.3;
    const int MAX = 4095;
    const float DIV = 5.15;
    digitalWrite(ADC_CTRL, LOW); 
    delay(20);
    int raw = analogRead(VBAT);
    digitalWrite(ADC_CTRL, HIGH);
    return (uint16_t)((raw * VREF / MAX) * DIV * 1000);
}

// ─────────────────────────────────────────
//                 SETUP
// ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("Starting Setup of Remote...");

    pinMode(ADC_CTRL, OUTPUT); digitalWrite(ADC_CTRL, HIGH);
    pinMode(VEXT, OUTPUT); digitalWrite(VEXT, LOW);
    analogReadResolution(12);

    displayManager.begin();
    drawForState();

    if (!loraManager.begin()) {
        Serial.println("LoRa Manager init failed!");
    }
    
    loraManager.onDisplayUpdate(onLoraDisplayUpdate);
    loraManager.onAckForValue(onLoraAckForValue);

    // Button setup is now simpler, we just poll them in the loop
    upButton.onPress(incPct); // For menu
    upButton.onHold(incPct, 200); // For menu
    downButton.onPress(decPct); // For menu
    downButton.onHold(decPct, 200); // For menu
    
    // The heartbeat task is no longer needed as KEEPALIVE serves a more specific purpose
    xTaskCreatePinnedToCore(heartbeatTask, "HeartbeatTask", 2048, NULL, 2, &heartbeatTaskHandle, 0);
    
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
            if (anyButtonPressed) {
                stateManager.switchToArming();
                armingStartTime = millis();
                Serial.println("IDLE → ARMING");
            }
            // Allow menu entry from IDLE via triple tap on UP button
            if(upButton.isPressed()){
                 static unsigned long lastUpPressTime = 0;
                 if(millis() - lastUpPressTime > 300) { // Basic debounce
                    registerTripleTap();
                    lastUpPressTime = millis();
                 }
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
                if (noButtonPressStartTime == 0) {
                    // Start the timer
                    noButtonPressStartTime = millis();
                    Serial.println("No button press timer started...");
                } else if (millis() - noButtonPressStartTime >= Config::NO_BUTTON_TIMEOUT_MS) {
                    Serial.println("CRUISING → IDLE (STOP_MOTOR sent)");
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