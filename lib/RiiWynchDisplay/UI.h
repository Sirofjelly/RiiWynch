#pragma once
#include <U8g2lib.h>

namespace RiiWynch {
namespace UI {

    /**
     * @brief Draws a standardized rounded frame.
     * @param u8g2 A reference to the U8G2 display object.
     */
    void drawFrame(U8G2& u8g2);

    /**
     * @brief Draws a standardized percentage bar.
     * @param u8g2 A reference to the U8G2 display object.
     * @param percentage The value (0-100) to display.
     */
    void drawBar(U8G2& u8g2, int percentage);

    /**
     * @brief Draws a 3-bar signal strength indicator.
     * @param u8g2 A reference to the U8G2 display object.
     * @param x The x-coordinate for the indicator.
     * @param y The y-coordinate for the indicator.
     * @param rssi The RSSI value in dBm.
     */
    void drawSignalStrength(U8G2& u8g2, int x, int y, float rssi);

} // namespace UI
} // namespace RiiWynch 