#pragma once
#include <RadioLib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "Protocol.h"

// LoRa pinout definitions
#define L_CS   8
#define L_DIO1 14
#define L_RST  12
#define L_BUSY 13
#define VEXT   21

// Message priority levels for queue handling
enum class MessagePriority : uint8_t {
    LOW_PRIORITY = 0,        // HEARTBEAT, regular messages
    NORMAL_PRIORITY = 1,     // KEEPALIVE, VALUE_SET, DISPLAY_UPDATE
    HIGH_PRIORITY = 2,       // ACK messages, MODE_UPDATE  
    CRITICAL_PRIORITY = 3    // START_MOTOR, STOP_MOTOR (safety critical)
};

class LoRaTransceiver {
public:
    LoRaTransceiver();
    ~LoRaTransceiver();

    bool begin(float freq = 868.0, int power = 14, int sf = 8, int cr = 5, float bw = 125.0);
    bool transmit(const RiiWynch::Protocol::Message& msg);
    bool transmitWithPriority(const RiiWynch::Protocol::Message& msg, MessagePriority priority);
    bool isMessageAvailable();
    bool receive(RiiWynch::Protocol::Message& msg);
    float getRSSI();
    float getCurrentRSSI();
    void updateRealTimeRSSI();

private:
    Module* mod;
    SX1262 radio;
    SemaphoreHandle_t loraMutex;
    volatile bool messageReady;
    float lastRSSI;
    unsigned long lastRSSIUpdate;
    
    // Priority-based mutex acquisition
    bool acquireMutexWithPriority(MessagePriority priority, TickType_t maxWait);
    TickType_t getTimeoutForPriority(MessagePriority priority);
    MessagePriority getMessagePriority(const RiiWynch::Protocol::Message& msg);
    
    static void IRAM_ATTR onReceive();
    static LoRaTransceiver* instance;
}; 