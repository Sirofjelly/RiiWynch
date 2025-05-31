#include "LoRaManager.h"
#include "HeartbeatManager.h"
#include <Arduino.h>

// Static member definitions
LoRaManager* LoRaManager::instance = nullptr;
const float LoRaManager::FREQUENCY = 868.0;
const int LoRaManager::OUTPUT_POWER = 14;
const int LoRaManager::SPREADING_FACTOR = 8;
const int LoRaManager::CODING_RATE = 5;
const float LoRaManager::BANDWIDTH = 125.0;
const int LoRaManager::DEVICE_ID = 1; // Main display device ID

// LoRa pinout definitions
#define L_CS   8
#define L_DIO1 14
#define L_RST  12
#define L_BUSY 13
#define VEXT   21

LoRaManager::LoRaManager(StateManager& stateMgr, DisplayManager& displayMgr)
    : state(stateMgr), display(displayMgr), heartbeatManager(nullptr), messageReady(false),
      lastSentDisplayPct(-1), waitingForAck(false), lastSendTime(0) {
    instance = this;
    mod = new Module(L_CS, L_DIO1, L_RST, L_BUSY, SPI);
    radio = new SX1262(mod);
}

bool LoRaManager::begin() {
    // Enable LoRa power (VEXT)
    pinMode(VEXT, OUTPUT);
    digitalWrite(VEXT, LOW); // Enable LoRa power

    // Use correct SPI pins for Heltec WiFi LoRa 32 (V3)
    SPI.begin(9, 11, 10, 8); // SCK, MISO, MOSI, SS
    
    int err = radio->begin(FREQUENCY);
    if (err != RADIOLIB_ERR_NONE) {
        Serial.print("LoRa init failed: ");
        Serial.println(err);
        return false;
    }
    
    // Configure LoRa parameters
    radio->setOutputPower(OUTPUT_POWER);
    radio->setSpreadingFactor(SPREADING_FACTOR);
    radio->setCodingRate(CODING_RATE);
    radio->setBandwidth(BANDWIDTH);
    
    // Configure LoRa interrupt for non-blocking operation
    radio->setDio1Action(onReceive);
    
    // Start continuous receive mode
    int rxErr = radio->startReceive();
    if (rxErr == RADIOLIB_ERR_NONE) {
        Serial.println("LoRa ready - Interrupt mode enabled.");
    } else {
        Serial.print("LoRa startReceive failed: ");
        Serial.println(rxErr);
        return false;
    }

    // Create mutex for LoRa communication
    loraMutex = xSemaphoreCreateMutex();
    if (loraMutex == NULL) {
        Serial.println("Failed to create LoRa mutex!");
        return false;
    }

    return true;
}

void LoRaManager::setHeartbeatManager(HeartbeatManager* hbMgr) {
    heartbeatManager = hbMgr;
}

void LoRaManager::update() {
    // Handle received messages
    if (messageReady) {
        if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            messageReady = false; // Clear the flag
            
            // Read the received data - this is fast, no blocking
            int loraStatus = radio->readData((uint8_t*)rxBuffer, sizeof(rxBuffer) - 1);
            
            if (loraStatus == RADIOLIB_ERR_NONE) {
                rxBuffer[sizeof(rxBuffer) - 1] = '\0'; // Ensure null-terminated
                Serial.print("[LoRa RX] ");
                Serial.println(rxBuffer);
                parseMessage(rxBuffer);
            } else {
                Serial.printf("[LoRa RX] Read error: %d\n", loraStatus);
            }
            
            // Restart continuous receive for next message
            restartReceive();
            xSemaphoreGive(loraMutex);
        }
    }
    
    // Handle resend logic
    if (waitingForAck && millis() - lastSendTime > RESEND_INTERVAL) {
        sendDisplayPercentage(lastSentDisplayPct);
    }
}

void LoRaManager::sendDisplayPercentage(int percentage) {
    if (!waitingForAck || percentage != lastSentDisplayPct) {
        if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            char txBuf[24];
            snprintf(txBuf, sizeof(txBuf), "DSP,%d,%d", DEVICE_ID, percentage);
            if (transmitMessage(txBuf)) {
                lastSentDisplayPct = percentage;
                waitingForAck = true;
                lastSendTime = millis();
                Serial.printf("[LoRa TX to Remote] DSP,%d,%d (new value)\n", DEVICE_ID, percentage);
            }
            restartReceive();
            xSemaphoreGive(loraMutex);
        }
    } else if (waitingForAck && millis() - lastSendTime > RESEND_INTERVAL) {
        if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            char txBuf[24];
            snprintf(txBuf, sizeof(txBuf), "DSP,%d,%d", DEVICE_ID, lastSentDisplayPct);
            if (transmitMessage(txBuf)) {
                lastSendTime = millis();
                Serial.printf("[LoRa TX to Remote - RESEND] DSP,%d,%d\n", DEVICE_ID, lastSentDisplayPct);
            }
            restartReceive();
            xSemaphoreGive(loraMutex);
        }
    }
}

void LoRaManager::sendACK(int percentage) {
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        char ackBuf[24];
        snprintf(ackBuf, sizeof(ackBuf), "ACK,%d,%d", DEVICE_ID, percentage);
        if (transmitMessage(ackBuf)) {
            Serial.print("[LoRa TX] ");
            Serial.println(ackBuf);
        }
        restartReceive();
        xSemaphoreGive(loraMutex);
    }
}

bool LoRaManager::isMessageReady() {
    return messageReady;
}

void LoRaManager::parseMessage(const char* message) {
    bool messageProcessed = false;
    int srcId = -1;
    // Parse VAL,<srcId>,<pct>[,<pktCnt>] from remote
    int pct = -1;
    unsigned int pktCnt = 0;
    if ((sscanf(message, "VAL,%d,%d,%u", &srcId, &pct, &pktCnt) >= 2 || sscanf(message, "VAL,%d,%d", &srcId, &pct) == 2) && pct >= 0 && pct <= 100) {
        if (srcId != DEVICE_ID) {
            handleVALMessage(pct);
            messageProcessed = true;
        }
    }
    // Parse ACK,<srcId>,<pct> from remote
    int ackPct = -1;
    int ackSrcId = -1;
    if (!messageProcessed && sscanf(message, "ACK,%d,%d", &ackSrcId, &ackPct) == 2) {
        if (ackSrcId != DEVICE_ID) {
            handleACKMessage(ackPct);
            messageProcessed = true;
        }
    }
    // Parse HBT (Heartbeat) from remote
    if (!messageProcessed && strncmp(message, "HBT,", 4) == 0) {
        handleHeartbeat();
        messageProcessed = true;
    }
    // Parse BTN (Button state) from remote
    if (!messageProcessed && strncmp(message, "BTN,", 4) == 0) {
        int btnValue = -1;
        unsigned int btnPktCnt = 0;
        int btnSrcId = -1;
        if (sscanf(message, "BTN,%d,%d,%u", &btnSrcId, &btnValue, &btnPktCnt) == 3) {
            if (btnSrcId != DEVICE_ID) {
                handleButtonMessage(btnValue);
            }
        }
        messageProcessed = true;
    }
    // Handle DSP messages (echo/loopback detection)
    int dspSrcId = -1;
    int dspPct = -1;
    if (!messageProcessed && sscanf(message, "DSP,%d,%d", &dspSrcId, &dspPct) == 2) {
        if (dspSrcId == DEVICE_ID) {
            // Self echo/loopback - silently ignore (no logging to reduce noise)
            static unsigned long selfEchoCount = 0;
            selfEchoCount++;
            // Only log occasionally for debugging purposes
            if (selfEchoCount % 100 == 0) {
                Serial.printf("[LoRa RX] Self-echo count: %lu\n", selfEchoCount);
            }
        } else {
            Serial.println("[LoRa RX] Ignoring DSP message (not for this device)");
        }
        messageProcessed = true;
    }
    // Update heartbeat for any valid message
    if (messageProcessed && heartbeatManager) {
        heartbeatManager->onHeartbeatReceived();
    }
    if (!messageProcessed) {
        Serial.println("[LoRa RX] Unknown message format from remote.");
    }
}

void LoRaManager::handleVALMessage(int percentage) {
    // Only process VAL messages if remote is connected
    if (heartbeatManager && heartbeatManager->isRemoteConnected()) {
        Serial.printf("[LoRa RX] VAL message: %d%%, updating main state\n", percentage);
        
        // Main is the authority - update its state first
        state.setDirectPercentage(percentage); // Set both target and displayed immediately
        display.update(percentage); // Update display immediately
        
        // Send ACK to acknowledge receipt
        sendACK(percentage);
        
        // Send DSP to update remote's display (main is authoritative)
        // Use a small delay to avoid collision with ACK
        delay(10);
        sendDisplayPercentage(percentage);
    } else {
        Serial.println("[LoRa RX] Ignored VAL, remote not connected.");
    }
}

void LoRaManager::handleACKMessage(int percentage) {
    Serial.printf("[LoRa RX] Got ACK for %d\n", percentage);
    if (waitingForAck && percentage == lastSentDisplayPct) {
        waitingForAck = false;
    }
}

void LoRaManager::handleHeartbeat() {
    // Heartbeat is handled by HeartbeatManager via onHeartbeatReceived call in parseMessage
    Serial.println("[LoRa RX] Heartbeat processed");
}

void LoRaManager::handleButtonMessage(int value) {
    Serial.printf("[LoRa RX] Button state: %d\n", value);
    // Button message handling can be added here if needed
}

bool LoRaManager::transmitMessage(const char* message) {
    int txResult = radio->transmit((uint8_t*)message, strlen(message));
    if (txResult == RADIOLIB_ERR_NONE) {
        return true;
    } else {
        Serial.printf("[LoRa TX] Error: %d\n", txResult);
        return false;
    }
}

void LoRaManager::restartReceive() {
    radio->startReceive();
}

void IRAM_ATTR LoRaManager::onReceive() {
    if (instance) {
        instance->messageReady = true;
    }
} 