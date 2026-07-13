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

    // Validate the message type against the full enum range.
    // The previous check only allowed up to MODE_UPDATE, which incorrectly
    // rejected newer ACK messages such as ACK_START_MOTOR.
    const auto typeValue = static_cast<uint8_t>(msg.type);
    const auto maxValue = static_cast<uint8_t>(MessageType::ACK_REMOTE_SETTINGS);
    if (typeValue == static_cast<uint8_t>(MessageType::INVALID) || typeValue > maxValue) {
        return false;
    }
    return true;
}

} // namespace Protocol
} // namespace RiiWynch 