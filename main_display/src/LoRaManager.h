#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <RadioLib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "StateManager.h"
#include "DisplayManager.h"

class HeartbeatManager; // Forward declaration

class LoRaManager {
public:
    LoRaManager(StateManager& stateMgr, DisplayManager& displayMgr);
    
    bool begin();
    void update();
    void sendDisplayPercentage(int percentage);
    void sendACK(int percentage);
    bool isMessageReady();
    void setHeartbeatManager(HeartbeatManager* hbMgr);
    SemaphoreHandle_t getMutex() { return loraMutex; }
    
private:
    // LoRa hardware
    Module* mod;
    SX1262* radio;
    
    // References to other managers
    StateManager& state;
    DisplayManager& display;
    HeartbeatManager* heartbeatManager;
    
    // Communication state
    char rxBuffer[64];
    volatile bool messageReady;
    int lastSentDisplayPct;
    bool waitingForAck;
    unsigned long lastSendTime;
    static const unsigned long RESEND_INTERVAL = 200; // ms
    
    // Thread safety
    SemaphoreHandle_t loraMutex;
    
    // Message parsing
    void parseMessage(const char* message);
    void handleVALMessage(int percentage);
    void handleACKMessage(int percentage);
    void handleHeartbeat();
    void handleButtonMessage(int value);
    
    // Transmission helpers
    bool transmitMessage(const char* message);
    void restartReceive();
    
    // Interrupt handler
    static void IRAM_ATTR onReceive();
    static LoRaManager* instance; // For interrupt handler
    
    // Constants
    static const float FREQUENCY;
    static const int OUTPUT_POWER;
    static const int SPREADING_FACTOR;
    static const int CODING_RATE;
    static const float BANDWIDTH;
};

#endif // LORA_MANAGER_H 