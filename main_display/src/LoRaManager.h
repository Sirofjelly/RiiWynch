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
    void sendStartAccepted(); // Confirm that a remote start request was accepted
    void sendStopMotor();   // Notify remote that motor has stopped
    float getRSSI();        // Access raw radio RSSI
    float getCurrentRSSI(); // RSSI from last valid Remote packet
    void updateRealTimeRSSI(); // Marks RSSI stale when no packet arrived recently
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

    // State for reliably syncing Remote Stop Delay
    bool waitingForRemoteSettingsAck = false;
    unsigned long lastRemoteSettingsSendTime = 0;
    unsigned long lastRemoteSettingsValue = 0;
    static const unsigned long REMOTE_SETTINGS_RESEND_INTERVAL = 2000; // ms

    // Mutex to protect ACK/resend state from race conditions
    SemaphoreHandle_t dspStateMutex = NULL;
}; 