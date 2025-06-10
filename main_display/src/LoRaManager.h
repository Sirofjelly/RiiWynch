#pragma once

#include "StateManager.h"
#include "DisplayManager.h"
#include "LoRaTransceiver.h"
#include "Protocol.h"

class HeartbeatManager; // Forward declaration

class LoRaManager {
public:
    LoRaManager(StateManager& stateMgr, DisplayManager& displayMgr);
    
    bool begin();
    void update();
    void sendDisplayPercentage(int percentage);
    void setHeartbeatManager(HeartbeatManager* hbMgr);

    bool getStartMotorRequest();
    bool getStopMotorRequest();
    
private:
    void handleMessage(const RiiWynch::Protocol::Message& msg);
    void sendAck(RiiWynch::Protocol::MessageType type, uint8_t percentage);

    LoRaTransceiver transceiver;
    StateManager& state;
    DisplayManager& display;
    HeartbeatManager* heartbeatManager;
    
    uint16_t packetCounter = 0;
    
    bool _startMotorRequest = false;
    bool _stopMotorRequest = false;
    
    // State for resending DSP messages
    bool waitingForDspAck = false;
    uint8_t lastSentDspPct = 0;
    unsigned long lastDspSendTime = 0;
    static const unsigned long DSP_RESEND_INTERVAL = 200; // ms
}; 