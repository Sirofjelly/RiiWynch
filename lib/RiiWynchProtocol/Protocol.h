#pragma once
#include <stdint.h>

namespace RiiWynch {
namespace Protocol {

enum class MessageType : uint8_t {
    INVALID = 0,
    HEARTBEAT,
    VALUE_SET,
    DISPLAY_UPDATE,
    ACK_VAL,
    ACK_DSP,
    BUTTON_PRESS,
    START_MOTOR,
    STOP_MOTOR,
    KEEPALIVE,
    LORA_SETTINGS,
    REMOTE_SETTINGS
};

enum class DeviceID : uint8_t {
    MAIN_DISPLAY = 1,
    REMOTE = 2,
};

#pragma pack(push, 1)
struct LoRaSettings {
    float frequency;      // MHz
    int16_t power;        // dBm (using int16_t to fit in packet)
    uint8_t spreadingFactor;
    uint8_t codingRate;
    float bandwidth;      // kHz
};

struct RemoteSettings {
    uint32_t stopDelayMs; // Configurable delay before stopping motor (ms)
};

struct Message {
    MessageType type;
    DeviceID source;
    uint16_t packetCounter;
    
    union Payload {
        uint8_t percentage;
        bool isPressed;
        LoRaSettings loraSettings;
        RemoteSettings remoteSettings;
    } payload;
};
#pragma pack(pop)

} // namespace Protocol
} // namespace RiiWynch 