#pragma once

#include "StateManager.h"
#include "DisplayManager.h"
#include "LoRaTransceiver.h"
#include "Protocol.h"
#include <FreeRTOS.h>
#include <semphr.h>

class HeartbeatManager; // Forward declaration

class LoRaManager {
public:
    LoRaManager(StateManager& stateMgr, DisplayManager& displayMgr);
    ~LoRaManager();

    bool begin();
    bool restart(); // Restart LoRa with new settings
    void update();
    void sendDisplayPercentage(int percentage);
    void sendModeUpdate(uint8_t modeIdx); // 🔄 New: send current mode to remote
    void sendLoRaSettings(); // Send current LoRa settings to remote
    void sendRemoteSettings(); // Send remote-specific settings to remote
    void sendStopMotor();   // Notify remote that motor has stopped
    float getRSSI();        // Access last RSSI value from transceiver
    float getCurrentRSSI(); // New: Get real-time RSSI
    void updateRealTimeRSSI(); // New: Update real-time RSSI reading
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
    
    // State for resending DSP messages (protected by dspStateMutex)
    bool waitingForDspAck = false;
    uint8_t lastSentDspPct = 0;
    unsigned long lastDspSendTime = 0;
    static const unsigned long DSP_RESEND_INTERVAL = 200; // ms

    // Mutex to protect ACK/resend state from race conditions
    SemaphoreHandle_t dspStateMutex = NULL;
}; 