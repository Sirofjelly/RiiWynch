#include "LoRaManager_remote.h"
#include "Settings_remote.h"
#include <Arduino.h>

LoRaManager_remote::LoRaManager_remote() {}

bool LoRaManager_remote::begin() {
    // Use global LoRa settings from Settings_remote.h
    Serial.printf("[LoRa Remote] Initializing with freq=%.1f MHz, power=%d dBm, SF=%d, CR=%d, BW=%.1f kHz\n", 
                  loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
    return transceiver.begin(loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
}

bool LoRaManager_remote::restart() {
    Serial.println("[LoRa Remote] Restarting with new settings...");
    // Use updated global LoRa settings
    Serial.printf("[LoRa Remote] New settings: freq=%.1f MHz, power=%d dBm, SF=%d, CR=%d, BW=%.1f kHz\n", 
                  loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
    return transceiver.begin(loraFrequency, loraPower, loraSpreadingFactor, loraCodingRate, loraBandwidth);
}

void LoRaManager_remote::onDisplayUpdate(void (*callback)(int percentage, float rssi)) {
    _displayUpdateCallback = callback;
}

void LoRaManager_remote::onAckForValue(void (*callback)(int percentage)) {
    _ackForValueCallback = callback;
}

void LoRaManager_remote::onLoRaSettingsReceived(void (*callback)()) {
    _loraSettingsReceivedCallback = callback;
}

void LoRaManager_remote::update() {
    RiiWynch::Protocol::Message msg;
    if (transceiver.receive(msg)) {
        handleMessage(msg);
    }

    // Handle resend logic for value set
    if (waitingForValAck && (millis() - lastValSendTime > VAL_RESEND_INTERVAL)) {
        Serial.printf("[LORA] Resending VAL: %d%%\n", lastSentValPct);
        sendValue(lastSentValPct);
    }
}

void LoRaManager_remote::handleMessage(const RiiWynch::Protocol::Message& msg) {
    if (msg.source != RiiWynch::Protocol::DeviceID::MAIN_DISPLAY) {
        return;
    }
    
    switch (msg.type) {
        case RiiWynch::Protocol::MessageType::DISPLAY_UPDATE:
            Serial.printf("[LORA RX] DSP: %d%%\n", msg.payload.percentage);
            sendAck(RiiWynch::Protocol::MessageType::ACK_DSP, msg.payload.percentage);
            if (_displayUpdateCallback) {
                _displayUpdateCallback(msg.payload.percentage, transceiver.getRSSI());
            }
            break;

        case RiiWynch::Protocol::MessageType::ACK_VAL:
             if (waitingForValAck && msg.payload.percentage == lastSentValPct) {
                Serial.printf("[LORA RX] ACK for VAL %d%% received.\n", msg.payload.percentage);
                waitingForValAck = false;
                if (_ackForValueCallback) {
                    _ackForValueCallback(msg.payload.percentage);
                }
            }
            break;

        case RiiWynch::Protocol::MessageType::LORA_SETTINGS:
            Serial.println("[LORA RX] LoRa settings received from main");
            // Apply the received LoRa settings
            applyReceivedLoRaSettings(
                msg.payload.loraSettings.frequency,
                (int)msg.payload.loraSettings.power,
                (int)msg.payload.loraSettings.spreadingFactor,
                (int)msg.payload.loraSettings.codingRate,
                msg.payload.loraSettings.bandwidth
            );
            // Notify callback that settings were received and applied
            if (_loraSettingsReceivedCallback) {
                _loraSettingsReceivedCallback();
            }
            break;

        default:
            Serial.printf("[LORA RX] Unknown message type: %d\n", static_cast<uint8_t>(msg.type));
            break;
    }
}

void LoRaManager_remote::sendValue(uint8_t percentage) {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::VALUE_SET;
    msg.source = RiiWynch::Protocol::DeviceID::REMOTE;
    msg.packetCounter = packetCounter++;
    msg.payload.percentage = percentage;

    if (transceiver.transmit(msg)) {
        Serial.printf("[LORA TX] VAL: %d%%\n", percentage);
        lastSentValPct = percentage;
        lastValSendTime = millis();
        waitingForValAck = true;
    } else {
        Serial.printf("[LORA TX] Failed to send VAL: %d%%\n", percentage);
    }
}

void LoRaManager_remote::sendButtonPress(bool isPressed) {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::BUTTON_PRESS;
    msg.source = RiiWynch::Protocol::DeviceID::REMOTE;
    msg.packetCounter = packetCounter++;
    msg.payload.isPressed = isPressed;
    transceiver.transmit(msg);
}

void LoRaManager_remote::sendHeartbeat() {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::HEARTBEAT;
    msg.source = RiiWynch::Protocol::DeviceID::REMOTE;
    msg.packetCounter = packetCounter++;
    transceiver.transmit(msg);
}

void LoRaManager_remote::sendStartMotor() {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::START_MOTOR;
    msg.source = RiiWynch::Protocol::DeviceID::REMOTE;
    msg.packetCounter = packetCounter++;
    if (!transceiver.transmit(msg)) {
        Serial.println("[LORA TX] Failed to send START_MOTOR");
    }
}

void LoRaManager_remote::sendStopMotor() {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::STOP_MOTOR;
    msg.source = RiiWynch::Protocol::DeviceID::REMOTE;
    msg.packetCounter = packetCounter++;
    if (!transceiver.transmit(msg)) {
        Serial.println("[LORA TX] Failed to send STOP_MOTOR");
    }
}

void LoRaManager_remote::sendKeepalive() {
    RiiWynch::Protocol::Message msg;
    msg.type = RiiWynch::Protocol::MessageType::KEEPALIVE;
    msg.source = RiiWynch::Protocol::DeviceID::REMOTE;
    msg.packetCounter = packetCounter++;
    if (!transceiver.transmit(msg)) {
        Serial.println("[LORA TX] Failed to send KEEPALIVE");
    }
}

void LoRaManager_remote::sendAck(RiiWynch::Protocol::MessageType type, uint8_t percentage) {
    RiiWynch::Protocol::Message msg;
    msg.type = type;
    msg.source = RiiWynch::Protocol::DeviceID::REMOTE;
    msg.packetCounter = packetCounter++;
    msg.payload.percentage = percentage;
    transceiver.transmit(msg);
} 