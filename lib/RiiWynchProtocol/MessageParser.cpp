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
    // A simple validation to check if the message type is within the defined enum range
    if (msg.type > MessageType::REMOTE_SETTINGS) {
        return false;
    }
    return true;
}

} // namespace Protocol
} // namespace RiiWynch 