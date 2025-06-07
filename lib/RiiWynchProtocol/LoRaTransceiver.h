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

class LoRaTransceiver {
public:
    LoRaTransceiver();
    ~LoRaTransceiver();

    bool begin(float freq = 868.0, int power = 14, int sf = 8, int cr = 5, float bw = 125.0);
    bool transmit(const RiiWynch::Protocol::Message& msg);
    bool isMessageAvailable();
    bool receive(RiiWynch::Protocol::Message& msg);
    float getRSSI();

private:
    Module* mod;
    SX1262 radio;
    SemaphoreHandle_t loraMutex;
    volatile bool messageReady;
    
    static void IRAM_ATTR onReceive();
    static LoRaTransceiver* instance;
}; 