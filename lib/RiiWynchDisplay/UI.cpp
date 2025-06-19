#include "UI.h"
#include <Arduino.h> // For constrain

namespace RiiWynch {
namespace UI {

void drawFrame(U8G2& u8g2) {
    u8g2.drawRBox(0, 0, 128, 64, 8);
    u8g2.setDrawColor(0); // color for the inner, clearing box
    u8g2.drawRBox(3, 3, 122, 58, 5);
    u8g2.setDrawColor(1); // Set color back to white for other elements
}

} // namespace UI
} // namespace RiiWynch 