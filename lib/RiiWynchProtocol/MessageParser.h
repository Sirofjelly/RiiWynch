#pragma once
#include "Protocol.h"
#include <stddef.h> // for size_t

namespace RiiWynch {
namespace Protocol {

class MessageParser {
public:
    static size_t serialize(const Message& msg, uint8_t* buffer, size_t bufferSize);
    static bool deserialize(const uint8_t* buffer, size_t bufferSize, Message& msg);
};

} // namespace Protocol
} // namespace RiiWynch 