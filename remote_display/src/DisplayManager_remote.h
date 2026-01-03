#pragma once
#include <U8g2lib.h>
#include "StateManager_remote.h"
#include "SnakeGame.h"

class DisplayManager_remote {
public:
    DisplayManager_remote();
    
    void begin();
    
    void drawStartScreen(int percentage, float rssi, uint16_t battery_mv, StateManager_remote::State currentState, const char* mode, bool showDelay = false, unsigned long delayMs = 1000);
    void drawMenuScreen(int percentage);
    void drawGameScreen(const SnakeGame& game, uint16_t highScore);
    void drawGameOverScreen(uint16_t score, uint16_t highScore, bool isNewHighScore);
    
private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
}; 