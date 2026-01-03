#include "LoRaManager.h"
#include "HeartbeatManager.h"
#include "ProfileManager.h"
#include "Settings.h"
#include <Arduino.h>

// Forward declarations for global accessors
extern ProfileManager& getGlobalProfileManager();

LoRaManager::LoRaManager(StateManager& stateMgr, DisplayManager& displayMgr)
    : state(stateMgr), display(displayMgr), heartbeatManager(nullptr) {}

LoRaManager::~LoRaManager() {
    if (dspStateMutex != NULL) {
        vSemaphoreDelete(dspStateMutex);
        dspStateMutex = NULL;
    }
}

bool LoRaManager::begin() {
    // Create mutex for protecting DSP ACK/resend state
    dspStateMutex = xSemaphoreCreateMutex();
    if (dspStateMutex == NULL) {
        Serial.println("[LoRa] Failed to create DSP state mutex!");
        return false;
    }

    // Use global LoRa settings from Settings.h
    Serial.printf("[LoRa] Initializing with freq=%.1f MHz, power=%d dBm, SF=%d, CR=%d, BW=%.1f kHz\n",
                  loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
    return transceiver.begin(loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
}

bool LoRaManager::restart() {
    Serial.println("[LoRa] Restarting with new settings...");
    // Use updated global LoRa settings
    Serial.printf("[LoRa] New settings: freq=%.1f MHz, power=%d dBm, SF=%d, CR=%d, BW=%.1f kHz\n", 
                  loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
    return transceiver.begin(loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
}

void LoRaManager::setHeartbeatManager(HeartbeatManager* hbMgr) {
    heartbeatManager = hbMgr;
}

void LoRaManager::update() {
    RiiWynch::Protocol::Message msg;
    if (transceiver.receive(msg)) {
        // Any valid message from remote resets the heartbeat
        if (heartbeatManager && msg.source == RiiWynch::Protocol::DeviceID::REMOTE) {
            heartbeatManager->onHeartbeatReceived();
        }
        handleMessage(msg);
    }

    // Handle resend logic for display updates (mutex-protected to avoid race with ACK handling)
    bool shouldResend = false;
    uint8_t pctToResend = 0;

    if (dspStateMutex != NULL && xSemaphoreTake(dspStateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (waitingForDspAck && (millis() - lastDspSendTime > DSP_RESEND_INTERVAL)) {
            shouldResend = true;
            pctToResend = lastSentDspPct;
        }
        xSemaphoreGive(dspStateMutex);
    }

    // Perform resend outside mutex to avoid holding lock during I/O
    if (shouldResend) {
        Serial.printf("[LORA] Resending DSP: %d%%\n", pctToResend);
        sendDisplayPercentage(pctToResend);
    }
}

void LoRaManager::handleMessage(const RiiWynch::Protocol::Message& msg) {
    // Ignore messages not from the remote
    if (msg.source != RiiWynch::Protocol::DeviceID::REMOTE) {
        return;
    }
    
    switch (msg.type) {
        case RiiWynch::Protocol::MessageType::VALUE_SET:
            if (heartbeatManager && heartbeatManager->isRemoteConnected()) {
                Serial.printf("[LORA RX] VAL: %d%%\n", msg.payload.percentage);
                state.setDirectPercentage(msg.payload.percentage);
                display.update(msg.payload.percentage, getCurrentRSSI(), getGlobalProfileManager().getCurrentModeName(), heartbeatManager->isRemoteConnected());
                sendAck(RiiWynch::Protocol::MessageType::ACK_VAL, msg.payload.percentage);
                
                // Also send a DSP update back to confirm the state
                vTaskDelay(pdMS_TO_TICKS(20));
                sendDisplayPercentage(msg.payload.percentage);

                // 🔄 Additionally, send the current mode so remote stays in sync
                sendModeUpdate(static_cast<uint8_t>(getGlobalProfileManager().getCurrentProfile()));
            } else {
                Serial.println("[LORA RX] Ignored VAL, remote not connected.");
            }
            break;

        case RiiWynch::Protocol::MessageType::ACK_DSP:
            // Mutex-protected ACK handling to avoid race with resend logic
            if (dspStateMutex != NULL && xSemaphoreTake(dspStateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                if (waitingForDspAck && msg.payload.percentage == lastSentDspPct) {
                    Serial.printf("[LORA RX] ACK for DSP %d%% received.\n", msg.payload.percentage);
                    waitingForDspAck = false;
                }
                xSemaphoreGive(dspStateMutex);
            }
            break;

        case RiiWynch::Protocol::MessageType::HEARTBEAT:
            // Heartbeat is implicitly handled by the check at the start of update()
            break;

        case RiiWynch::Protocol::MessageType::BUTTON_PRESS:
            // Button press handling can be added here if needed in the future
            Serial.printf("[LORA RX] BTN: %s\n", msg.payload.isPressed ? "pressed" : "released");
            break;

        case RiiWynch::Protocol::MessageType::START_MOTOR:
            Serial.println("[LORA RX] START_MOTOR");
            _startMotorRequest = true;
            break;

        case RiiWynch::Protocol::MessageType::STOP_MOTOR:
            Serial.println("[LORA RX] STOP_MOTOR");
            _stopMotorRequest = true;
            break;

        case RiiWynch::Protocol::MessageType::KEEPALIVE:
            // Also handled implicitly, but good to have an explicit case
            break;

        default:
            Serial.printf("[LORA RX] Unknown message type: %d\n", static_cast<uint8_t>(msg.type));
            break;
    }
}

void LoRaManager::sendDisplayPercentage(int percentage) {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::DISPLAY_UPDATE;
    msg.source = RiiWynch::Protocol::DeviceID::MAIN_DISPLAY;
    msg.packetCounter = packetCounter++;
    msg.payload.percentage = percentage;

    if (transceiver.transmit(msg)) {
        Serial.printf("[LORA TX] DSP: %d%%\n", percentage);
        // Mutex-protected state update to avoid race with ACK handling and resend logic
        if (dspStateMutex != NULL && xSemaphoreTake(dspStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            lastSentDspPct = percentage;
            lastDspSendTime = millis();
            waitingForDspAck = true;
            xSemaphoreGive(dspStateMutex);
        }
    } else {
        Serial.printf("[LORA TX] Failed to send DSP: %d%%\n", percentage);
    }
}

void LoRaManager::sendLoRaSettings() {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::LORA_SETTINGS;
    msg.source = RiiWynch::Protocol::DeviceID::MAIN_DISPLAY;
    msg.packetCounter = packetCounter++;
    
    // Pack current LoRa settings into the message
    msg.payload.loraSettings.frequency = loraFrequency;
    msg.payload.loraSettings.power = (int16_t)loraPower;
    msg.payload.loraSettings.spreadingFactor = (uint8_t)loraSpreadingFactor;
    msg.payload.loraSettings.codingRate = (uint8_t)loraCodingRate;
    msg.payload.loraSettings.bandwidth = loraBandwidth;

    if (transceiver.transmit(msg)) {
        Serial.printf("[LORA TX] Settings sent: freq=%.1f MHz, power=%d dBm, SF=%d, CR=%d, BW=%.1f kHz\n", 
                      loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
    } else {
        Serial.println("[LORA TX] Failed to send LoRa settings");
    }
}

void LoRaManager::sendRemoteSettings() {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::REMOTE_SETTINGS;
    msg.source = RiiWynch::Protocol::DeviceID::MAIN_DISPLAY;
    msg.packetCounter = packetCounter++;
    
    // Pack current remote settings into the message
    msg.payload.remoteSettings.stopDelayMs = remoteStopDelayMs;

    if (transceiver.transmit(msg)) {
        Serial.printf("[LORA TX] Remote settings sent: stopDelayMs=%lu ms\n", remoteStopDelayMs);
    } else {
        Serial.println("[LORA TX] Failed to send remote settings");
    }
}

void LoRaManager::sendAck(RiiWynch::Protocol::MessageType type, uint8_t percentage) {
    RiiWynch::Protocol::Message msg;
    msg.type = type;
    msg.source = RiiWynch::Protocol::DeviceID::MAIN_DISPLAY;
    msg.packetCounter = packetCounter++;
    msg.payload.percentage = percentage;

    if (transceiver.transmit(msg)) {
        Serial.printf("[LORA TX] ACK: %d%%\n", percentage);
    } else {
        Serial.printf("[LORA TX] Failed to send ACK for %d%%\n", percentage);
    }
}

void LoRaManager::sendModeUpdate(uint8_t modeIdx) {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::MODE_UPDATE;
    msg.source = RiiWynch::Protocol::DeviceID::MAIN_DISPLAY;
    msg.packetCounter = packetCounter++;
    msg.payload.modeIndex = modeIdx;

    if (transceiver.transmit(msg)) {
        Serial.printf("[LORA TX] MODE_UPDATE: %d\n", modeIdx);
    } else {
        Serial.println("[LORA TX] Failed to send MODE_UPDATE");
    }
}

bool LoRaManager::getStartMotorRequest() {
    if (_startMotorRequest) {
        _startMotorRequest = false;
        return true;
    }
    return false;
}

bool LoRaManager::getStopMotorRequest() {
    if (_stopMotorRequest) {
        _stopMotorRequest = false;
        return true;
    }
    return false;
}

void LoRaManager::sendStopMotor() {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::STOP_MOTOR;
    msg.source = RiiWynch::Protocol::DeviceID::MAIN_DISPLAY;
    msg.packetCounter = packetCounter++;

    if (transceiver.transmitWithPriority(msg, MessagePriority::CRITICAL_PRIORITY)) {
        Serial.println("[LORA TX] STOP_MOTOR");
    } else {
        Serial.println("[LORA TX] Failed to send STOP_MOTOR");
    }
}

float LoRaManager::getRSSI() {
    return transceiver.getRSSI();
}

float LoRaManager::getCurrentRSSI() {
    return transceiver.getCurrentRSSI();
}

void LoRaManager::updateRealTimeRSSI() {
    transceiver.updateRealTimeRSSI();
} 