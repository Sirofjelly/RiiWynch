#include "DisplayManager_remote.h"
#include "UI.h"

DisplayManager_remote::DisplayManager_remote() 
    : u8g2(U8G2_R0, U8X8_PIN_NONE, 20, 19) {}

void DisplayManager_remote::begin() {
    u8g2.begin();
}

void DisplayManager_remote::drawStartScreen(int percentage, float rssi, uint16_t battery_mv) {
    u8g2.clearBuffer();
    RiiWynch::UI::drawFrame(u8g2);
    
    // Title
    u8g2.setFont(u8g2_font_logisoso28_tf);
    int w = u8g2.getStrWidth("START");
    u8g2.drawStr((128 - w) / 2, 46, "START");

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
    RiiWynch::UI::drawSignalStrength(u8g2, rssiX + rssiWidth + 5, 13, rssi);

    // Battery
    char batBuf[8];
    sprintf(batBuf, "%.2fV", battery_mv / 1000.0);
    u8g2.drawStr(128 - u8g2.getStrWidth(batBuf) - 6, 13, batBuf);
    
    u8g2.sendBuffer();
}

void DisplayManager_remote::drawMenuScreen(int percentage) {
    u8g2.clearBuffer(); 
    RiiWynch::UI::drawFrame(u8g2);
    
    char txt[6];
    if (percentage == 0) strcpy(txt, "STOP");
    else sprintf(txt, "%d%%", percentage);
    
    u8g2.setFont(u8g2_font_logisoso42_tf);
    int w = u8g2.getStrWidth(txt);
    u8g2.drawStr((128 - w) / 2, 47, txt);
    
    RiiWynch::UI::drawBar(u8g2, percentage);
    
    u8g2.sendBuffer();
} 