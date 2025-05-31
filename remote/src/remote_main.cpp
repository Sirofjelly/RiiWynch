#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <RadioLib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <stdarg.h>

static const int DEVICE_ID = 2; // Remote device ID

// ─────────────────────────────────────────
//         BASIC FORWARD DECLARATIONS
// ─────────────────────────────────────────
void drawStart();
void drawMenu();
void incPct();
void decPct();
uint16_t readBattery();
void parseLoRaMessage(const char* buffer);
void registerTripleTap();
void heartbeatTask(void *parameter);
bool transmitLoRaMessage(const char* format, ...);

// ─────────────────────────────────────────
//            CONFIGURATION CONSTANTS
// ─────────────────────────────────────────
namespace Config {
    // Timing constants
    static const unsigned long DEBOUNCE_MS = 20;
    static const unsigned long REPEAT_MS = 200;
    static const unsigned long MENU_TIMEOUT_MS = 1500;
    static const unsigned long START_UPDATE_MS = 500;
    static const unsigned long HEARTBEAT_INTERVAL = 500;
    static const unsigned long TRIPLE_TAP_WINDOW = 1500;
    
    // Display constants
    static const int PERCENTAGE_STEP = 5;
    static const int SMOOTH_UPDATE_MS = 50;
    static const int SMOOTH_STEP = 5;
    
    // LoRa constants
    static const float LORA_FREQUENCY = 868.0;
    static const int LORA_OUTPUT_POWER = 14;
    static const int LORA_SPREADING_FACTOR = 8;
    static const int LORA_CODING_RATE = 5;
    static const float LORA_BANDWIDTH = 125.0;
    static const int LORA_MUTEX_TIMEOUT = 50;
}

// ─── Display ───
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, 20, 19);

// ─── LoRa (SX1262) ───
#define L_CS   8
#define L_DIO1 14
#define L_RST  12
#define L_BUSY 13
Module  mod(L_CS, L_DIO1, L_RST, L_BUSY, SPI);
SX1262  radio(&mod);

// ─── FreeRTOS Task Handles ───
TaskHandle_t heartbeatTaskHandle = NULL;

// ─── Mutexes for thread-safe LoRa access ───
SemaphoreHandle_t loraMutex = NULL;

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
    
    ButtonState(int p) : pin(p), currentState(HIGH), lastState(HIGH), 
                        debounceTime(0), pressTime(0), actionFunc(nullptr) {}
};

// ─── Forward declarations that depend on ButtonState ───
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
char txBuf[32];
uint16_t pktCnt = 0;
static bool lastReportedAnyPressedState = false;

// ─── LoRa Non-Blocking Receive ───
volatile bool loraReceivedFlag = false;
volatile bool loraEnableInterrupt = true;

void IRAM_ATTR loraIsr() {
    if (loraEnableInterrupt) {
        loraReceivedFlag = true;
    }
}

// ─────────────────────────────────────────
//           UNIFIED LORA TRANSMISSION
// ─────────────────────────────────────────
bool transmitLoRaMessage(const char* format, ...) {
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(Config::LORA_MUTEX_TIMEOUT)) == pdTRUE) {
        va_list args;
        va_start(args, format);
        vsnprintf(txBuf, sizeof(txBuf), format, args);
        va_end(args);
        
        loraEnableInterrupt = false;
        int result = radio.transmit((uint8_t *)txBuf, strlen(txBuf));
        
        int startRxState = radio.startReceive();
        if (startRxState != RADIOLIB_ERR_NONE) {
            Serial.printf("[LoRa TX] startReceive failed: %d\n", startRxState);
        }
        
        loraEnableInterrupt = true;
        xSemaphoreGive(loraMutex);
        
        return (result == RADIOLIB_ERR_NONE);
    }
    return false;
}

// ─── Simplified LoRa Send Functions ───
void sendBtn(uint8_t v) { transmitLoRaMessage("BTN,%d,%d,%u", DEVICE_ID, v, pktCnt++); }
void sendPct(int p) { transmitLoRaMessage("VAL,%d,%d,%u", DEVICE_ID, p, pktCnt++); }
void sendLoraAck(int pct) { transmitLoRaMessage("ACK,%d,%d", DEVICE_ID, pct); }

// ─────────────────────────────────────────
//            MESSAGE PARSING
// ─────────────────────────────────────────
void parseLoRaMessage(const char* buffer) {
    int srcId = -1;
    int pct = -1;
    bool messageProcessed = false;
    
    // Parse DSP message from main display
    if (sscanf(buffer, "DSP,%d,%d", &srcId, &pct) == 2 && pct >= 0 && pct <= 100) {
        if (srcId != DEVICE_ID) {
            Serial.printf("[LoRa RX] DSP: %d\n", pct);
            if (state != MENU) {  // Only update if not in menu mode
                targetPct = shownPct = pct;
            }
            sendLoraAck(pct);
        } else {
            Serial.println("[LoRa RX] Ignoring DSP message (self echo/loopback)");
        }
        messageProcessed = true;
    }
    
    // Parse ACK message
    int ackSrcId = -1;
    int ackPct = -1;
    if (!messageProcessed && sscanf(buffer, "ACK,%d,%d", &ackSrcId, &ackPct) == 2) {
        if (ackSrcId != DEVICE_ID) {
            Serial.printf("[LoRa RX] ACK: %d\n", ackPct);
        } else {
            Serial.println("[LoRa RX] Ignoring ACK message (self echo/loopback)");
        }
        messageProcessed = true;
    }
    
    // Parse VAL message
    int valSrcId = -1;
    int valPct = -1;
    unsigned int valPktCnt = 0;
    if (!messageProcessed && (sscanf(buffer, "VAL,%d,%d,%u", &valSrcId, &valPct, &valPktCnt) >= 2 || sscanf(buffer, "VAL,%d,%d", &valSrcId, &valPct) == 2)) {
        if (valSrcId != DEVICE_ID) {
            Serial.printf("[LoRa RX] VAL: %d\n", valPct);
        } else {
            Serial.println("[LoRa RX] Ignoring VAL message (self echo/loopback)");
        }
        messageProcessed = true;
    }
    
    // Parse BTN message
    int btnSrcId = -1;
    int btnValue = -1;
    unsigned int btnPktCnt = 0;
    if (!messageProcessed && sscanf(buffer, "BTN,%d,%d,%u", &btnSrcId, &btnValue, &btnPktCnt) == 3) {
        if (btnSrcId != DEVICE_ID) {
            Serial.printf("[LoRa RX] BTN: %d\n", btnValue);
        } else {
            Serial.println("[LoRa RX] Ignoring BTN message (self echo/loopback)");
        }
        messageProcessed = true;
    }
    
    if (!messageProcessed) {
        Serial.printf("[LoRa RX] Unknown: %s\n", buffer);
    }
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
            if (millis() - btn.pressTime < 500 && btn.actionFunc) {
                btn.actionFunc();
            }
            lastChangeTime = 0;
        }
    }
    
    // Handle button repeat for long press
    if (btn.currentState == LOW && millis() - btn.pressTime >= 500 && 
        millis() - lastChangeTime > Config::REPEAT_MS && btn.actionFunc) {
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
        sendBtn(0);
        state = MENU;
        lastActivity = millis();
        drawMenu();
    }
}

// ─── Action Functions ───
void incPct() {
    if (targetPct < 100) {
        targetPct += Config::PERCENTAGE_STEP;
        drawMenu();
        Serial.printf("INC → %d\n", targetPct);
    }
}

void decPct() {
    if (targetPct > 0) {
        targetPct -= Config::PERCENTAGE_STEP;
        drawMenu();
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
        
        if (state == START || state == MENU) {
            transmitLoRaMessage("HBT,%u", pktCnt++);
        }
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

    char buf[8];
    sprintf(buf, "%.2fV", readBattery() / 1000.0);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf) - 6, 13, buf);
    
    u8g2.sendBuffer();
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

    // Initialize pins
    pinMode(UP_BTN, INPUT_PULLUP);
    pinMode(DOWN_BTN, INPUT_PULLUP);
    pinMode(ADC_CTRL, OUTPUT); digitalWrite(ADC_CTRL, HIGH);
    pinMode(VEXT, OUTPUT); digitalWrite(VEXT, LOW);
    analogReadResolution(12);

    // Initialize display
    u8g2.begin();
    drawStart();

    // Initialize LoRa
    SPI.begin();
    int err = radio.begin(Config::LORA_FREQUENCY);
    if (err != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa init failed: %d\n", err);
    } else {
        radio.setOutputPower(Config::LORA_OUTPUT_POWER);
        radio.setSpreadingFactor(Config::LORA_SPREADING_FACTOR);
        radio.setCodingRate(Config::LORA_CODING_RATE);
        radio.setBandwidth(Config::LORA_BANDWIDTH);
        radio.setDio1Action(loraIsr);
        
        int startRxState = radio.startReceive();
        if (startRxState == RADIOLIB_ERR_NONE) {
            Serial.println("LoRa ready.");
        } else {
            Serial.printf("LoRa startReceive failed: %d\n", startRxState);
        }
    }

    // Create mutex and task
    loraMutex = xSemaphoreCreateMutex();
    if (loraMutex == NULL) {
        Serial.println("Failed to create LoRa mutex!");
    }

    // Set up button action functions
    upButton.actionFunc = incPct;
    downButton.actionFunc = decPct;

    // Create heartbeat task
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
    // Handle LoRa messages
    if (loraReceivedFlag) {
        if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            loraEnableInterrupt = false;
            loraReceivedFlag = false;

            char loraRxBuf[32];
            int packetLength = radio.getPacketLength();
            size_t readLen = min(packetLength, (int)(sizeof(loraRxBuf) - 1));

            if (radio.readData((uint8_t*)loraRxBuf, readLen) == RADIOLIB_ERR_NONE) {
                loraRxBuf[readLen] = '\0';
                parseLoRaMessage(loraRxBuf);
            }

            radio.startReceive();
            loraEnableInterrupt = true;
            xSemaphoreGive(loraMutex);
        }
    }

    // State machine
    switch (state) {
        case START: {
            // Handle triple-tap detection on UP button
            if (updateButtonState(upButton) && upButton.currentState == LOW) {
                registerTripleTap();
            }
            updateButtonState(downButton); // Still need to update for proper state tracking

            // Report button state to main display
            bool currentAnyPressed = (digitalRead(UP_BTN) == LOW || digitalRead(DOWN_BTN) == LOW);
            if (currentAnyPressed != lastReportedAnyPressedState) {
                sendBtn(currentAnyPressed ? 1 : 0);
                lastReportedAnyPressedState = currentAnyPressed;
            }

            // Periodic display update
            if (millis() - lastStartUpdate >= Config::START_UPDATE_MS) {
                drawStart();
                lastStartUpdate = millis();
            }
            break;
        }

        case MENU: {
            // Check for activity to reset timeout
            if (digitalRead(UP_BTN) == LOW || digitalRead(DOWN_BTN) == LOW) {
                lastActivity = millis();
            }
            
            // Menu timeout check
            if (millis() - lastActivity > Config::MENU_TIMEOUT_MS) {
                sendPct(targetPct);
                state = START;
                drawStart();
                lastStartUpdate = millis();
                break;
            }

            // Update buttons
            updateButtonState(upButton);
            updateButtonState(downButton);

            // Smooth percentage animation
            if (millis() - lastUpdateTime > Config::SMOOTH_UPDATE_MS && shownPct != targetPct) {
                shownPct += (shownPct < targetPct) ? Config::SMOOTH_STEP : -Config::SMOOTH_STEP;
                
                // Prevent overshoot
                if ((shownPct > targetPct && targetPct < shownPct - Config::SMOOTH_STEP) || 
                    (shownPct < targetPct && targetPct > shownPct + Config::SMOOTH_STEP)) {
                    shownPct = targetPct;
                }
                
                drawMenu();
                lastUpdateTime = millis();
            }
            break;
        }
    }
}