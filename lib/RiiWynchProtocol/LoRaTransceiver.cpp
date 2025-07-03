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
    // Create mutex with priority inheritance to prevent priority inversion
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

MessagePriority LoRaTransceiver::getMessagePriority(const RiiWynch::Protocol::Message& msg) {
    switch (msg.type) {
        case RiiWynch::Protocol::MessageType::START_MOTOR:
        case RiiWynch::Protocol::MessageType::STOP_MOTOR:
            return MessagePriority::CRITICAL_PRIORITY;
            
        case RiiWynch::Protocol::MessageType::ACK_VAL:
        case RiiWynch::Protocol::MessageType::ACK_DSP:
        case RiiWynch::Protocol::MessageType::MODE_UPDATE:
            return MessagePriority::HIGH_PRIORITY;
            
        case RiiWynch::Protocol::MessageType::KEEPALIVE:
        case RiiWynch::Protocol::MessageType::VALUE_SET:
        case RiiWynch::Protocol::MessageType::DISPLAY_UPDATE:
        case RiiWynch::Protocol::MessageType::LORA_SETTINGS:
        case RiiWynch::Protocol::MessageType::REMOTE_SETTINGS:
            return MessagePriority::NORMAL_PRIORITY;
            
        case RiiWynch::Protocol::MessageType::HEARTBEAT:
        case RiiWynch::Protocol::MessageType::BUTTON_PRESS:
        default:
            return MessagePriority::LOW_PRIORITY;
    }
}

TickType_t LoRaTransceiver::getTimeoutForPriority(MessagePriority priority) {
    switch (priority) {
        case MessagePriority::CRITICAL_PRIORITY:
            return pdMS_TO_TICKS(1000);  // 2 seconds for safety-critical messages
        case MessagePriority::HIGH_PRIORITY:
            return pdMS_TO_TICKS(500);   // 500ms for important messages
        case MessagePriority::NORMAL_PRIORITY:
            return pdMS_TO_TICKS(100);   // 100ms for normal messages
        case MessagePriority::LOW_PRIORITY:
        default:
            return pdMS_TO_TICKS(50);    // 50ms for low priority messages
    }
}

bool LoRaTransceiver::acquireMutexWithPriority(MessagePriority priority, TickType_t maxWait) {
    // For critical messages, temporarily raise task priority to ensure they get through
    UBaseType_t originalPriority = 0;
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    
    if (priority == MessagePriority::CRITICAL_PRIORITY) {
        originalPriority = uxTaskPriorityGet(currentTask);
        // Temporarily raise to highest priority to ensure critical messages get through
        vTaskPrioritySet(currentTask, configMAX_PRIORITIES - 1);
        Serial.printf("[Transceiver] CRITICAL message - raised task priority from %d to %d\n", 
                     originalPriority, configMAX_PRIORITIES - 1);
    }
    
    bool acquired = xSemaphoreTake(loraMutex, maxWait) == pdTRUE;
    
    // Restore original priority after attempting to acquire mutex
    if (priority == MessagePriority::CRITICAL_PRIORITY && originalPriority > 0) {
        vTaskPrioritySet(currentTask, originalPriority);
        Serial.printf("[Transceiver] Restored task priority to %d\n", originalPriority);
    }
    
    return acquired;
}

bool LoRaTransceiver::transmit(const RiiWynch::Protocol::Message& msg) {
    MessagePriority priority = getMessagePriority(msg);
    return transmitWithPriority(msg, priority);
}

bool LoRaTransceiver::transmitWithPriority(const RiiWynch::Protocol::Message& msg, MessagePriority priority) {
    TickType_t timeout = getTimeoutForPriority(priority);
    
    if (priority == MessagePriority::CRITICAL_PRIORITY) {
        Serial.printf("[Transceiver] CRITICAL message type %d - using %dms timeout\n", 
                     static_cast<int>(msg.type), pdTICKS_TO_MS(timeout));
    }
    
    if (!acquireMutexWithPriority(priority, timeout)) {
        Serial.printf("[Transceiver TX] Failed to get mutex for priority %d message (type %d) after %dms\n", 
                     static_cast<int>(priority), static_cast<int>(msg.type), pdTICKS_TO_MS(timeout));
        
        // For critical messages, this is a severe error that needs attention
        if (priority == MessagePriority::CRITICAL_PRIORITY) {
            Serial.println("[Transceiver TX] !!! CRITICAL MESSAGE FAILED - SAFETY COMPROMISED !!!");
        }
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
            if (priority == MessagePriority::CRITICAL_PRIORITY) {
                Serial.printf("[Transceiver] CRITICAL message type %d sent successfully\n", 
                             static_cast<int>(msg.type));
            }
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