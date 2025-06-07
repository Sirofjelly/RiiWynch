#include "LoRaTransceiver.h"
#include "MessageParser.h"
#include <Arduino.h>

LoRaTransceiver* LoRaTransceiver::instance = nullptr;

LoRaTransceiver::LoRaTransceiver()
    : mod(new Module(L_CS, L_DIO1, L_RST, L_BUSY, SPI)), 
      radio(mod),
      messageReady(false) {
    instance = this;
}

LoRaTransceiver::~LoRaTransceiver() {
    vSemaphoreDelete(loraMutex);
    delete mod;
}

void IRAM_ATTR LoRaTransceiver::onReceive() {
    if (instance) {
        instance->messageReady = true;
    }
}

bool LoRaTransceiver::begin(float freq, int power, int sf, int cr, float bw) {
    loraMutex = xSemaphoreCreateMutex();
    if (loraMutex == NULL) {
        Serial.println("[Transceiver] Failed to create LoRa mutex!");
        return false;
    }
    
    // Enable LoRa power (VEXT)
    pinMode(VEXT, OUTPUT);
    digitalWrite(VEXT, LOW); // Enable LoRa power

    // Use correct SPI pins for Heltec WiFi LoRa 32 (V3)
    SPI.begin(9, 11, 10, 8); // SCK, MISO, MOSI, SS

    int state = radio.begin(freq);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[Transceiver] LoRa init failed: %d\n", state);
        return false;
    }
    
    radio.setOutputPower(power);
    radio.setSpreadingFactor(sf);
    radio.setCodingRate(cr);
    radio.setBandwidth(bw);
    radio.setDio1Action(onReceive);

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[Transceiver] LoRa startReceive failed: %d\n", state);
        return false;
    }
    return true;
}

bool LoRaTransceiver::transmit(const RiiWynch::Protocol::Message& msg) {
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        Serial.println("[Transceiver TX] Failed to get mutex");
        return false;
    }

    uint8_t buffer[sizeof(RiiWynch::Protocol::Message)];
    size_t len = RiiWynch::Protocol::MessageParser::serialize(msg, buffer, sizeof(buffer));

    bool result = false;
    if (len > 0) {
        // Disable interrupt while transmitting and starting receive again
        radio.setDio1Action(nullptr);
        int txResult = radio.transmit(buffer, len);
        radio.startReceive();
        radio.setDio1Action(onReceive);
        
        if (txResult != RADIOLIB_ERR_NONE) {
            Serial.printf("[Transceiver TX] Error: %d\n", txResult);
        } else {
            result = true;
        }
    }

    xSemaphoreGive(loraMutex);
    return result;
}

bool LoRaTransceiver::isMessageAvailable() {
    return messageReady;
}

bool LoRaTransceiver::receive(RiiWynch::Protocol::Message& msg) {
    if (!messageReady) {
        return false;
    }

    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false; // Could not get mutex, try again later
    }
    
    messageReady = false; 

    radio.setDio1Action(nullptr); // Disable interrupt while processing

    int packetLength = radio.getPacketLength();
    uint8_t buffer[sizeof(RiiWynch::Protocol::Message)];
    bool success = false;

    if (packetLength != sizeof(buffer)) {
        Serial.printf("[Transceiver RX] Invalid packet size: %d, expected %d. Flushing.\n", packetLength, sizeof(buffer));
    } else {
        int state = radio.readData(buffer, packetLength);
        if (state == RADIOLIB_ERR_NONE) {
            if (RiiWynch::Protocol::MessageParser::deserialize(buffer, packetLength, msg)) {
                success = true;
            } else {
                Serial.println("[Transceiver RX] Deserialization failed.");
            }
        } else {
            Serial.printf("[Transceiver RX] Read error: %d\n", state);
        }
    }

    // Go back to listening for another packet
    radio.startReceive();
    radio.setDio1Action(onReceive);
    xSemaphoreGive(loraMutex);

    return success;
}

float LoRaTransceiver::getRSSI() {
    return radio.getRSSI();
} 