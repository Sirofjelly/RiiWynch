#include "DisplayManager_remote.h"
#include "UI.h"

DisplayManager_remote::DisplayManager_remote() 
    : u8g2(U8G2_R0, U8X8_PIN_NONE, 20, 19) {}

void DisplayManager_remote::begin() {
    u8g2.begin();
}

void DisplayManager_remote::drawStartScreen(int percentage, float rssi, uint16_t battery_mv, StateManager_remote::State currentState, const char* mode, bool showDelay, unsigned long delayMs) {
    u8g2.clearBuffer();
    RiiWynch::UI::drawFrame(u8g2);
    
    // Title
    u8g2.setFont(u8g2_font_logisoso28_tf);
    const char* title = "START";
    switch (currentState) {
        case StateManager_remote::State::IDLE:
            title = "STOP";
            break;
        case StateManager_remote::State::ARMING:
            title = "ARMING";
            break;
        case StateManager_remote::State::CRUISING:
            title = "CRUISE";
            break;
        case StateManager_remote::State::MENU: // Should not be drawn via this function, but handle it
             title = "MENU";
             break;
    }
    int w = u8g2.getStrWidth(title);
    u8g2.drawStr((128 - w) / 2, 46, title);

    // Top status bar
    u8g2.setFont(u8g2_font_6x10_tf);

    // Percentage
    char pctBuf[8];
    sprintf(pctBuf, "%d%%", percentage);
    u8g2.drawStr(6, 13, pctBuf);

    // RSSI
    char rssiBuf[10];
    sprintf(rssiBuf, "%.0fdBm", rssi);
    int rssiWidth = u8g2.getStrWidth(rssiBuf);
    int rssiX = (128 - rssiWidth - 20) / 2;
    u8g2.drawStr(rssiX, 13, rssiBuf);

    // Battery
    char batBuf[8];
    sprintf(batBuf, "%.2fV", battery_mv / 1000.0);
    u8g2.drawStr(128 - u8g2.getStrWidth(batBuf) - 6, 13, batBuf);
    
    // Delay indicator at bottom center
    if (showDelay) {
        u8g2.setFont(u8g2_font_6x10_tf);
        char delayText[16];
        if (delayMs >= 1000) {
            sprintf(delayText, "%.1fs Delay", delayMs / 1000.0);
        } else {
            sprintf(delayText, "%lums Delay", delayMs);
        }
        int delayWidth = u8g2.getStrWidth(delayText);
        u8g2.drawStr((128 - delayWidth) / 2, 58, delayText);
    }
    
    // 🔄 Draw current mode at bottom left
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(6, 58, mode);
    
    u8g2.sendBuffer();
}

void DisplayManager_remote::drawMenuScreen(int percentage) {
    u8g2.clearBuffer(); 
    RiiWynch::UI::drawFrame(u8g2);
    
    char txt[6];
    sprintf(txt, "%d%%", percentage);
    
    u8g2.setFont(u8g2_font_logisoso38_tf);
    int w = u8g2.getStrWidth(txt);
    u8g2.drawStr((128 - w) / 2, 53, txt);
    
    u8g2.sendBuffer();
} 