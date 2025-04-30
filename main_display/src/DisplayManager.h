#pragma once
#include <U8g2lib.h>

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void update(int percentage);
  void updateText(const char* text);  // ✅ Public flashing display
  void clear();                       // ✅ Public clear screen
  void init();
  void blinkStopText(bool show);

private:
  void drawFrame();
  void drawBar(int percentage);
  void drawThickerRoundedFrame();
  void drawSlimRoundedBar(int percentage);
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
};
