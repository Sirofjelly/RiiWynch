#include "MessageParser.h"
#include <string.h>

namespace RiiWynch {
namespace Protocol {

size_t MessageParser::serialize(const Message& msg, uint8_t* buffer, size_t bufferSize) {
    if (bufferSize < sizeof(Message)) {
        return 0; // Not enough space
    }
    memcpy(buffer, &msg, sizeof(Message));
    return sizeof(Message);
}

bool MessageParser::deserialize(const uint8_t* buffer, size_t bufferSize, Message& msg) {
    if (bufferSize < sizeof(Message)) {
        return false; // Not enough data
    }
    memcpy(&msg, buffer, sizeof(Message));

    // Validate explicitly so newly-added message types after MODE_UPDATE are not
    // accidentally rejected (this broke ACK_START_MOTOR and ACK_REMOTE_SETTINGS).
    switch (msg.type) {
        case MessageType::HEARTBEAT:
        case MessageType::VALUE_SET:
        case MessageType::DISPLAY_UPDATE:
        case MessageType::ACK_VAL:
        case MessageType::ACK_DSP:
        case MessageType::BUTTON_PRESS:
        case MessageType::START_MOTOR:
        case MessageType::STOP_MOTOR:
        case MessageType::KEEPALIVE:
        case MessageType::LORA_SETTINGS:
        case MessageType::REMOTE_SETTINGS:
        case MessageType::MODE_UPDATE:
        case MessageType::ACK_START_MOTOR:
        case MessageType::ACK_REMOTE_SETTINGS:
            return true;
        case MessageType::INVALID:
        default:
            return false;
    }
}

} // namespace Protocol
} // namespace RiiWynch 