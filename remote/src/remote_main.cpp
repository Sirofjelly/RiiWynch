#include <Arduino.h>
#include <Wire.h>
#include <FreeRTOS.h>
#include "LoRaManager_remote.h"
#include <RiiWynchInput/Button.h>
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
    static const unsigned long HEARTBEAT_INTERVAL = 500;
    static const unsigned long TRIPLE_TAP_WINDOW = 1500;
    static const int PERCENTAGE_STEP = 5;
    static const int SMOOTH_UPDATE_MS = 50;
    static const int SMOOTH_STEP = 5;
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

// ─── Application State ───
static bool lastReportedAnyPressedState = false;
float currentRSSI = 0.0f;
unsigned long lastStartUpdate = 0;

// ─────────────────────────────────────────
//           LORA CALLBACKS
// ─────────────────────────────────────────

void onLoraDisplayUpdate(int percentage, float rssi) {
    currentRSSI = rssi;
    Serial.printf("[LORA CB] DSP: %d%% (current state: %s, targetPct: %d, shownPct: %d)\n", 
                 percentage, (stateManager.getState() == StateManager_remote::State::START) ? "START" : "MENU", stateManager.getTargetPercentage(), stateManager.getShownPercentage());

    if (stateManager.getState() != StateManager_remote::State::MENU) {
        stateManager.setTargetPercentage(percentage);
        Serial.printf("[Remote] Updated display to %d%% from main\n", percentage);
    } else {
        Serial.println("[Remote] Ignoring DSP update value - in MENU mode, but ACK sent.");
    }
}

void onLoraAckForValue(int percentage) {
    Serial.printf("[Remote] Received ACK from Main for VAL %d%%\n", percentage);
    // This callback confirms receipt, the manager handles the waiting state.
}

// ─────────────────────────────────────────
//           BUTTON HANDLING
// ─────────────────────────────────────────
void registerTripleTap() {
    static unsigned long tapTimes[3] = {0, 0, 0};
    const unsigned long TRIPLE_TAP_WINDOW = 1500;

    tapTimes[0] = tapTimes[1];
    tapTimes[1] = tapTimes[2];
    tapTimes[2] = millis();

    if (tapTimes[0] > 0 && (tapTimes[2] - tapTimes[0] <= TRIPLE_TAP_WINDOW)) {
        Serial.println("TRIPLE PRESS → MENU");
        tapTimes[0] = tapTimes[1] = tapTimes[2] = 0;
        loraManager.sendButtonPress(true);
        stateManager.switchToMenu();
        drawMenu();
    }
}

void incPct() {
    stateManager.increasePercentage(Config::PERCENTAGE_STEP);
    Serial.printf("INC → %d\n", stateManager.getTargetPercentage());
}

void decPct() {
    stateManager.decreasePercentage(Config::PERCENTAGE_STEP);
    Serial.printf("DEC → %d\n", stateManager.getTargetPercentage());
}

// ─────────────────────────────────────────
//            HEARTBEAT TASK
// ─────────────────────────────────────────
void heartbeatTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t heartbeatPeriod = pdMS_TO_TICKS(Config::HEARTBEAT_INTERVAL);
    
    for (;;) {
        vTaskDelayUntil(&lastWakeTime, heartbeatPeriod);
        loraManager.sendHeartbeat();
    }
}

// ─────────────────────────────────────────
//              DISPLAY LOGIC
// ─────────────────────────────────────────
void drawMenu() {
    displayManager.drawMenuScreen(stateManager.getShownPercentage());
}

void drawStart() {
    displayManager.drawStartScreen(stateManager.getShownPercentage(), currentRSSI, readBattery());
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
    drawStart();

    if (!loraManager.begin()) {
        Serial.println("LoRa Manager init failed!");
    }
    
    loraManager.onDisplayUpdate(onLoraDisplayUpdate);
    loraManager.onAckForValue(onLoraAckForValue);

    // Setup button callbacks
    upButton.onPress([]() {
        if (stateManager.getState() == StateManager_remote::State::START) {
            registerTripleTap();
        } else if (stateManager.getState() == StateManager_remote::State::MENU) {
            incPct();
        }
    });
    upButton.onHold([]() {
        if (stateManager.getState() == StateManager_remote::State::MENU) incPct();
    }, 200);

    downButton.onPress([]() {
        if (stateManager.getState() == StateManager_remote::State::MENU) decPct();
    });
    downButton.onHold([]() {
        if (stateManager.getState() == StateManager_remote::State::MENU) decPct();
    }, 200);

    xTaskCreatePinnedToCore(heartbeatTask, "HeartbeatTask", 2048, NULL, 2, &heartbeatTaskHandle, 0);
    
    if (heartbeatTaskHandle == NULL) {
        Serial.println("Failed to create heartbeat task!");
    } else {
        Serial.println("Setup complete - Heartbeat task created on core 0");
    }
}

// ─────────────────────────────────────────
//                 MAIN LOOP
// ─────────────────────────────────────────
void loop() {
    loraManager.update();
    upButton.update();
    downButton.update();

    switch (stateManager.getState()) {
        case StateManager_remote::State::START: {
            bool currentAnyPressed = upButton.isPressed() || downButton.isPressed();
            if (currentAnyPressed != lastReportedAnyPressedState) {
                loraManager.sendButtonPress(currentAnyPressed);
                lastReportedAnyPressedState = currentAnyPressed;
            }

            if (millis() - lastStartUpdate >= Config::START_UPDATE_MS) {
                drawStart();
                lastStartUpdate = millis();
            }
            break;
        }

        case StateManager_remote::State::MENU: {
            if (upButton.isPressed() || downButton.isPressed()) {
                stateManager.resetMenuActivityTimer();
            }
            
            if (stateManager.isMenuTimedOut(Config::MENU_TIMEOUT_MS)) {
                Serial.printf("[Remote] Menu timeout - sending VAL message with %d%% to main\n", stateManager.getTargetPercentage());
                loraManager.sendValue(stateManager.getTargetPercentage());
                stateManager.switchToStart();
                drawStart();
                lastStartUpdate = millis();
                break;
            }

            if (stateManager.updateShownPercentage(Config::SMOOTH_STEP, Config::SMOOTH_UPDATE_MS)) {
                drawMenu();
            }
            break;
        }
    }
    delay(5);
}