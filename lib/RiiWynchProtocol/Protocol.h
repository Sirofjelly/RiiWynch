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
};

enum class DeviceID : uint8_t {
    MAIN_DISPLAY = 1,
    REMOTE = 2,
};

#pragma pack(push, 1)
struct Message {
    MessageType type;
    DeviceID source;
    uint16_t packetCounter;
    
    union Payload {
        uint8_t percentage;
        bool isPressed;
    } payload;
};
#pragma pack(pop)

} // namespace Protocol
} // namespace RiiWynch 