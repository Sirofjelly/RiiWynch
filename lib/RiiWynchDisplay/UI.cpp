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

void drawBar(U8G2& u8g2, int percentage) {
    int barWidth = (percentage * 116) / 100;
    barWidth = constrain(barWidth, 0, 116);
    if (barWidth > 0) {
        // Draw the filled part of the bar
        u8g2.drawRBox(6, 51, barWidth, 6, barWidth < 6 ? barWidth / 2 : 3);
    }
    // Draw the outer frame of the bar
    u8g2.drawRFrame(5, 50, 118, 8, 3);
}

void drawSignalStrength(U8G2& u8g2, int x, int y, float rssi) {
    const int barWidth = 4;
    const int barSpacing = 2;
    const int maxHeight = 8;

    // Determine height of each bar based on RSSI thresholds
    int bar1Height = (rssi > -105) ? maxHeight / 3 : 0;
    int bar2Height = (rssi > -90) ? (maxHeight * 2) / 3 : 0;
    int bar3Height = (rssi > -75) ? maxHeight : 0;

    // Draw the three bars, adjusting for y-position since height grows upwards
    if (bar1Height > 0) u8g2.drawBox(x, y - bar1Height, barWidth, bar1Height);
    if (bar2Height > 0) u8g2.drawBox(x + barWidth + barSpacing, y - bar2Height, barWidth, bar2Height);
    if (bar3Height > 0) u8g2.drawBox(x + (barWidth + barSpacing) * 2, y - bar3Height, barWidth, bar3Height);
}

} // namespace UI
} // namespace RiiWynch 