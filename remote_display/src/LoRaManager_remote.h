#pragma once

#include "LoRaTransceiver.h"
#include "Protocol.h"

class LoRaManager_remote {
public:
    LoRaManager_remote();
    
    bool begin();
    void update();
    
    void sendValue(uint8_t percentage);
    void sendButtonPress(bool isPressed);
    void sendHeartbeat();
    void sendStartMotor();
    void sendStopMotor();
    void sendKeepalive();
    
    // Callback functions to link with main application logic
    void onDisplayUpdate(void (*callback)(int percentage, float rssi));
    void onAckForValue(void (*callback)(int percentage));

private:
    void handleMessage(const RiiWynch::Protocol::Message& msg);
    void sendAck(RiiWynch::Protocol::MessageType type, uint8_t percentage);

    LoRaTransceiver transceiver;
    uint16_t packetCounter = 0;

    // Callbacks
    void (*_displayUpdateCallback)(int percentage, float rssi) = nullptr;
    void (*_ackForValueCallback)(int percentage) = nullptr;
    
    // State for resending VAL messages
    bool waitingForValAck = false;
    uint8_t lastSentValPct = 0;
    unsigned long lastValSendTime = 0;
    static const unsigned long VAL_RESEND_INTERVAL = 2000; // ms
}; 