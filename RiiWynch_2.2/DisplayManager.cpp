#include "DisplayManager.h"

DisplayManager::DisplayManager()
  : u8g2(U8G2_R0, U8X8_PIN_NONE, 20, 19) {}

void DisplayManager::begin() {
  u8g2.begin();
}

void DisplayManager::update(int percentage) {
  u8g2.clearBuffer();
  drawFrame();

  u8g2.setFont(u8g2_font_logisoso42_tf);
  char text[6];
  if (percentage == 0) snprintf(text, sizeof(text), "STOP");
  else snprintf(text, sizeof(text), "%d%%", percentage);

  int16_t width = u8g2.getStrWidth(text);
  u8g2.drawStr((128 - width) / 2, 47, text);
  drawBar(percentage);

  u8g2.sendBuffer();
}

void DisplayManager::drawFrame() {
  u8g2.drawRBox(0, 0, 128, 64, 8);
  u8g2.setDrawColor(0);
  u8g2.drawRBox(3, 3, 122, 58, 5);
  u8g2.setDrawColor(1);
}

void DisplayManager::drawBar(int percentage) {
  int barWidth = (percentage * 116) / 100;
  barWidth = constrain(barWidth, 0, 116);
  if (barWidth > 0)
    u8g2.drawRBox(6, 51, barWidth, 6, barWidth < 6 ? barWidth / 2 : 3);
  u8g2.drawRFrame(5, 50, 118, 8, 3);
}

// ✅ NEW: Flashing STOP
void DisplayManager::updateText(const char* text) {
  u8g2.clearBuffer();
  drawFrame();
  u8g2.setFont(u8g2_font_logisoso42_tf);
  int16_t width = u8g2.getStrWidth(text);
  u8g2.drawStr((128 - width) / 2, 47, text);
  u8g2.sendBuffer();
}

// ✅ NEW: Clear screen
void DisplayManager::clear() {
  u8g2.clearDisplay();
}
