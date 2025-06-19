#pragma once
#include <U8g2lib.h>

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void update(int percentage, float rssi, const char* mode, bool isConnected);
  void updateText(const char* text);  // ✅ Public flashing display
  void clear();                       // ✅ Public clear screen
  void init();
  
  // Mode display protection methods
  void startModeDisplay(const char* modeText, unsigned long displayDuration = 1000);
  void updateModeDisplay(); // Call this in main loop to handle timeout
  bool isModeDisplayActive(); // Check if mode display is currently active

  // New: draw STOP or general status screen with small percentage and RSSI
  void drawStopScreen(int percentage, float rssi, const char* mode, bool isConnected);

private:
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
  
  // Mode display protection state
  bool _modeDisplayActive;
  unsigned long _modeDisplayStartTime;
  unsigned long _modeDisplayDuration;
  char _modeDisplayText[16]; // Store the mode text
};
