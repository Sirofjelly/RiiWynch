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
    sprintf(batBuf, "%.1fV", battery_mv / 1000.0);
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
    u8g2.drawStr((128 - w) / 2, 51, txt);
    u8g2.sendBuffer();
}

void DisplayManager_remote::drawGameScreen(const SnakeGame& game, uint16_t highScore) {
    u8g2.clearBuffer();
    
    // Header
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(4, 10);
    u8g2.print("Score: ");
    u8g2.print(game.getScore());
    
    u8g2.setCursor(70, 10);
    u8g2.print("HI: ");
    u8g2.print(highScore);
    
    // Horizontal line
    u8g2.drawLine(0, 12, 128, 12);
    
    // Draw Border for Game Area (120x48 px, centered horizontally)
    // Game area starts at X=4, Y=14
    // Box surrounding it: X=3, Y=13, W=122, H=50
    u8g2.drawFrame(3, 13, 122, 50);

    // Draw Snake
    const auto& snake = game.getSnake();
    for (const auto& p : snake) {
        // Offset X=4, Y=14. Cell=4
        u8g2.drawBox(4 + p.x * 4, 14 + p.y * 4, 4, 4);
    }
    
    // Draw Food
    Point food = game.getFood();
    int fx = 4 + food.x * 4;
    int fy = 14 + food.y * 4;
    
    if (game.getFoodType() == SnakeGame::FoodType::GOLD) {
        // Hollow Circle for Gold
        u8g2.drawCircle(fx + 2, fy + 2, 2); 
    } else {
        // Solid Circle for Normal
        u8g2.drawDisc(fx + 2, fy + 2, 1);
    }
    
    // Draw Shrink Pill
    if (game.isShrinkPillActive()) {
        Point pill = game.getShrinkPill();
        int px = 4 + pill.x * 4;
        int py = 14 + pill.y * 4;
        // Draw 'X'
        u8g2.drawLine(px, py, px + 3, py + 3);
        u8g2.drawLine(px + 3, py, px, py + 3);
    }
    
    u8g2.sendBuffer();
}

void DisplayManager_remote::drawGameOverScreen(uint16_t score, uint16_t highScore, bool isNewHighScore) {
    u8g2.clearBuffer();
    RiiWynch::UI::drawFrame(u8g2);
    
    u8g2.setFont(u8g2_font_ncenB14_tr);
    const char* title = "GAME OVER";
    int w = u8g2.getStrWidth(title);
    u8g2.drawStr((128 - w) / 2, 30, title);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    
    char buf[32];
    sprintf(buf, "Score: %d", score);
    w = u8g2.getStrWidth(buf);
    u8g2.drawStr((128 - w) / 2, 45, buf);
    
    if (isNewHighScore) {
        const char* newHi = "NEW HIGH SCORE!";
        w = u8g2.getStrWidth(newHi);
        u8g2.drawStr((128 - w) / 2, 60, newHi);
    } else {
        sprintf(buf, "High: %d", highScore);
        w = u8g2.getStrWidth(buf);
        u8g2.drawStr((128 - w) / 2, 60, buf);
    }
    
    u8g2.sendBuffer();
} 