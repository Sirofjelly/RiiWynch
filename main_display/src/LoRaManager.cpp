#include "LoRaManager.h"
#include "HeartbeatManager.h"
#include <Arduino.h>

LoRaManager::LoRaManager(StateManager& stateMgr, DisplayManager& displayMgr)
    : state(stateMgr), display(displayMgr), heartbeatManager(nullptr) {}

bool LoRaManager::begin() {
    return transceiver.begin();
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

    // Handle resend logic for display updates
    if (waitingForDspAck && (millis() - lastDspSendTime > DSP_RESEND_INTERVAL)) {
        Serial.printf("[LORA] Resending DSP: %d%%\n", lastSentDspPct);
        sendDisplayPercentage(lastSentDspPct);
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
                display.update(msg.payload.percentage);
                sendAck(RiiWynch::Protocol::MessageType::ACK_VAL, msg.payload.percentage);
                
                // Also send a DSP update back to confirm the state
                vTaskDelay(pdMS_TO_TICKS(20));
                sendDisplayPercentage(msg.payload.percentage);
            } else {
                Serial.println("[LORA RX] Ignored VAL, remote not connected.");
            }
            break;

        case RiiWynch::Protocol::MessageType::ACK_DSP:
            if (waitingForDspAck && msg.payload.percentage == lastSentDspPct) {
                Serial.printf("[LORA RX] ACK for DSP %d%% received.\n", msg.payload.percentage);
                waitingForDspAck = false;
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
            state.setState(StateManager::State::STOPPED);
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
        lastSentDspPct = percentage;
        lastDspSendTime = millis();
        waitingForDspAck = true;
    } else {
        Serial.printf("[LORA TX] Failed to send DSP: %d%%\n", percentage);
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