#pragma once
#include <U8g2lib.h>

class DisplayManager_remote {
public:
    DisplayManager_remote();
    
    void begin();
    
    void drawStartScreen(int percentage, float rssi, uint16_t battery_mv);
    void drawMenuScreen(int percentage);
    
private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
}; 