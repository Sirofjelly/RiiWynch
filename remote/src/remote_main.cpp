#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <FreeRTOS.h>
#include "LoRaManager_remote.h"

// ─────────────────────────────────────────
//         BASIC FORWARD DECLARATIONS
// ─────────────────────────────────────────
void drawStart();
void drawMenu();
void incPct();
void decPct();
uint16_t readBattery();
void registerTripleTap();
void heartbeatTask(void *parameter);
void drawSignalStrengthBars(int x, int y, float rssi);

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

// ─── Display ───
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 20, 19);

// ─── LoRa Manager ───
LoRaManager_remote loraManager;

// ─── FreeRTOS Task Handles ───
TaskHandle_t heartbeatTaskHandle = NULL;

// ─── Pins ───
#define UP_BTN    7
#define DOWN_BTN  4
#define VBAT      1
#define ADC_CTRL 37
#define VEXT     21

// ─── States ───
enum State { START, MENU };
State state = START;

// ─── Button State Structure ───
struct ButtonState {
    int pin;
    int currentState = HIGH;
    int lastState = HIGH;
    unsigned long debounceTime = 0;
    unsigned long pressTime = 0;
    void (*actionFunc)() = nullptr;
    ButtonState(int p) : pin(p) {}
};

bool updateButtonState(ButtonState& btn);
ButtonState upButton(UP_BTN);
ButtonState downButton(DOWN_BTN);

// ─── Application State ───
int targetPct = 0, shownPct = 0;
unsigned long lastChangeTime = 0;
unsigned long lastUpdateTime = 0;
unsigned long lastActivity = 0;
unsigned long lastStartUpdate = 0;
unsigned long tapTimes[3] = {0, 0, 0};
static bool lastReportedAnyPressedState = false;
float currentRSSI = 0.0f;

// ─────────────────────────────────────────
//           LORA CALLBACKS
// ─────────────────────────────────────────

void onLoraDisplayUpdate(int percentage, float rssi) {
    currentRSSI = rssi;
    Serial.printf("[LORA CB] DSP: %d%% (current state: %s, targetPct: %d, shownPct: %d)\n", 
                 percentage, (state == START) ? "START" : "MENU", targetPct, shownPct);

    if (state != MENU) {
        targetPct = shownPct = percentage;
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
bool updateButtonState(ButtonState& btn) {
    int reading = digitalRead(btn.pin);
    bool stateChanged = false;
    
    if (reading != btn.lastState) {
        btn.debounceTime = millis();
    }
    
    if (millis() - btn.debounceTime > Config::DEBOUNCE_MS && reading != btn.currentState) {
        btn.currentState = reading;
        stateChanged = true;
        
        if (btn.currentState == LOW) {
            btn.pressTime = millis();
            lastChangeTime = millis();
        } else {
            if (millis() - btn.pressTime < 500 && btn.actionFunc && state == MENU) {
                btn.actionFunc();
            }
            lastChangeTime = 0;
        }
    }
    
    if (btn.currentState == LOW && millis() - btn.pressTime >= 500 && 
        millis() - lastChangeTime > Config::REPEAT_MS && btn.actionFunc && state == MENU) {
        btn.actionFunc();
        lastChangeTime = millis();
    }
    
    btn.lastState = reading;
    return stateChanged;
}

void registerTripleTap() {
    tapTimes[0] = tapTimes[1];
    tapTimes[1] = tapTimes[2];
    tapTimes[2] = millis();

    if (tapTimes[0] > 0 && (tapTimes[2] - tapTimes[0] <= Config::TRIPLE_TAP_WINDOW)) {
        Serial.println("TRIPLE PRESS → MENU");
        tapTimes[0] = tapTimes[1] = tapTimes[2] = 0;
        loraManager.sendButtonPress(true); // Sending a generic "special" button press
        state = MENU;
        lastActivity = millis();
        drawMenu();
    }
}

void incPct() {
    if (targetPct < 100) {
        targetPct = min(100, targetPct + Config::PERCENTAGE_STEP);
        Serial.printf("INC → %d\n", targetPct);
    }
}

void decPct() {
    if (targetPct > 0) {
        targetPct = max(0, targetPct - Config::PERCENTAGE_STEP);
        Serial.printf("DEC → %d\n", targetPct);
    }
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
//              DISPLAY FUNCTIONS
// ─────────────────────────────────────────
void drawFrame() {
    const int t = 3, r = 8;
    u8g2.drawRBox(0, 0, 128, 64, r);
    u8g2.setDrawColor(0);
    u8g2.drawRBox(t, t, 128 - 2 * t, 64 - 2 * t, r - t);
    u8g2.setDrawColor(1);
}

void drawBar(int pct) {
    int x = 6, y = 51, h = 6, maxW = 116;
    int w = constrain(pct, 0, 100) * maxW / 100;
    int rad = (w < 6) ? w / 2 : 3;
    if (w > 0) u8g2.drawRBox(x, y, w, h, rad);
    u8g2.drawRFrame(x - 1, y - 1, maxW + 2, h + 2, 3);
}

void drawStart() {
    u8g2.clearBuffer(); 
    drawFrame();
    u8g2.setFont(u8g2_font_logisoso28_tf);
    int w = u8g2.getStrWidth("START");
    u8g2.drawStr((128 - w) / 2, 46, "START");

    char pctBuf[8];
    sprintf(pctBuf, "%d%%", shownPct);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(6, 13, pctBuf);

    char rssiBuf[10];
    sprintf(rssiBuf, "%.0fdBm", currentRSSI);
    u8g2.setFont(u8g2_font_6x10_tf);
    int rssiWidth = u8g2.getStrWidth(rssiBuf);
    int rssiX = (128 - rssiWidth - 20) / 2;
    u8g2.drawStr(rssiX, 13, rssiBuf);

    drawSignalStrengthBars(rssiX + rssiWidth + 5, 13, currentRSSI);

    char buf[8];
    sprintf(buf, "%.2fV", readBattery() / 1000.0);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf) - 6, 13, buf);
    
    u8g2.sendBuffer();
}

void drawSignalStrengthBars(int x, int y, float rssi) {
    int barWidth = 4;
    int barSpacing = 2;
    int maxHeight = 8;

    int bar1Height = (rssi > -105) ? maxHeight / 3 : 0;
    u8g2.drawBox(x, y - bar1Height, barWidth, bar1Height);

    int bar2Height = (rssi > -90) ? (maxHeight * 2) / 3 : 0;
    u8g2.drawBox(x + barWidth + barSpacing, y - bar2Height, barWidth, bar2Height);

    int bar3Height = (rssi > -75) ? maxHeight : 0;
    u8g2.drawBox(x + (barWidth + barSpacing) * 2, y - bar3Height, barWidth, bar3Height);
}

void drawMenu() {
    u8g2.clearBuffer(); 
    drawFrame();
    char txt[6];
    if (shownPct == 0) strcpy(txt, "STOP");
    else sprintf(txt, "%d%%", shownPct);
    u8g2.setFont(u8g2_font_logisoso42_tf);
    int w = u8g2.getStrWidth(txt);
    u8g2.drawStr((128 - w) / 2, 47, txt);
    drawBar(shownPct);
    u8g2.sendBuffer();
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

    pinMode(UP_BTN, INPUT_PULLUP);
    pinMode(DOWN_BTN, INPUT_PULLUP);
    pinMode(ADC_CTRL, OUTPUT); digitalWrite(ADC_CTRL, HIGH);
    pinMode(VEXT, OUTPUT); digitalWrite(VEXT, LOW);
    analogReadResolution(12);

    u8g2.begin();
    drawStart();

    if (!loraManager.begin()) {
        Serial.println("LoRa Manager init failed!");
        // Maybe hang here or indicate error on display
    }
    
    // Register callbacks
    loraManager.onDisplayUpdate(onLoraDisplayUpdate);
    loraManager.onAckForValue(onLoraAckForValue);

    upButton.actionFunc = incPct;
    downButton.actionFunc = decPct;

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

    switch (state) {
        case START: {
            if (updateButtonState(upButton) && upButton.currentState == LOW) {
                registerTripleTap();
            }
            updateButtonState(downButton);

            bool currentAnyPressed = (digitalRead(UP_BTN) == LOW || digitalRead(DOWN_BTN) == LOW);
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

        case MENU: {
            if (digitalRead(UP_BTN) == LOW || digitalRead(DOWN_BTN) == LOW) {
                lastActivity = millis();
            }
            
            if (millis() - lastActivity > Config::MENU_TIMEOUT_MS) {
                Serial.printf("[Remote] Menu timeout - sending VAL message with %d%% to main\n", targetPct);
                loraManager.sendValue(targetPct);
                state = START;
                drawStart();
                lastStartUpdate = millis();
                break;
            }

            updateButtonState(upButton);
            updateButtonState(downButton);

            if (millis() - lastUpdateTime > Config::SMOOTH_UPDATE_MS && shownPct != targetPct) {
                int step = (shownPct < targetPct) ? Config::SMOOTH_STEP : -Config::SMOOTH_STEP;
                shownPct += step;
                if ((step > 0 && shownPct > targetPct) || (step < 0 && shownPct < targetPct)) {
                    shownPct = targetPct;
                }
                drawMenu();
                lastUpdateTime = millis();
            }
            break;
        }
    }
    delay(5);
}