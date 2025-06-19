#include "DisplayManager.h"
#include "UI.h"
#include <string.h>

DisplayManager::DisplayManager()
  : u8g2(U8G2_R0, U8X8_PIN_NONE, 20, 19), 
    _modeDisplayActive(false), 
    _modeDisplayStartTime(0), 
    _modeDisplayDuration(0) {
  _modeDisplayText[0] = '\0'; // Initialize empty string
}

void DisplayManager::begin() {
  u8g2.begin();
}

void DisplayManager::update(int percentage, float rssi) {
  // Don't update if mode display is active
  if (_modeDisplayActive) {
    return;
  }
  
  u8g2.clearBuffer();
  RiiWynch::UI::drawFrame(u8g2);

  // ─── Top status bar ──────────────────────────────────────────────
  u8g2.setFont(u8g2_font_6x10_tf);

  // Percentage (small) on the left
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", percentage);
  u8g2.drawStr(6, 13, pctBuf);

  // RSSI (small) centred with signal icon to the right
  char rssiBuf[10];
  snprintf(rssiBuf, sizeof(rssiBuf), "%.0fdBm", rssi);
  int16_t rssiWidth = u8g2.getStrWidth(rssiBuf);
  int16_t rssiX = (128 - rssiWidth - 20) / 2;
  u8g2.drawStr(rssiX, 13, rssiBuf);
  RiiWynch::UI::drawSignalStrength(u8g2, rssiX + rssiWidth + 5, 13, rssi);

  // ─── Main large value ────────────────────────────────────────────
  u8g2.setFont(u8g2_font_logisoso38_tf);
  char mainText[6];
  snprintf(mainText, sizeof(mainText), "%d%%", percentage);

  int16_t width = u8g2.getStrWidth(mainText);
  u8g2.drawStr((128 - width) / 2, 55, mainText);

  u8g2.sendBuffer();
}

// ✅ NEW: Flashing STOP
void DisplayManager::updateText(const char* text) {
  // Don't update if mode display is active (unless it's being called by mode display itself)
  if (_modeDisplayActive) {
    return;
  }
  
  u8g2.clearBuffer();
  RiiWynch::UI::drawFrame(u8g2);
  // Use medium font and center for all mode labels (SURF, SKIM, SMOOTh, MANUAL)
  if (strcmp(text, "SURF") == 0 || strcmp(text, "SKIM") == 0 || strcmp(text, "SMOOTH") == 0 || strcmp(text, "MANUAL") == 0) {
    u8g2.setFont(u8g2_font_helvB18_tf); // Medium font
    int16_t width = u8g2.getStrWidth(text);
    u8g2.drawStr((128 - width) / 2, 44, text);
  } else {
    u8g2.setFont(u8g2_font_logisoso42_tf);
    int16_t width = u8g2.getStrWidth(text);
    u8g2.drawStr((128 - width) / 2, 47, text);
  }
  u8g2.sendBuffer();
}

// ✅ NEW: Clear screen
void DisplayManager::clear() {
  u8g2.clearDisplay();
}

// Mode display protection methods
void DisplayManager::startModeDisplay(const char* modeText, unsigned long displayDuration) {
  _modeDisplayActive = true;
  _modeDisplayStartTime = millis();
  _modeDisplayDuration = displayDuration;
  
  // Copy the mode text safely
  strncpy(_modeDisplayText, modeText, sizeof(_modeDisplayText) - 1);
  _modeDisplayText[sizeof(_modeDisplayText) - 1] = '\0';
  
  // Display the mode text immediately
  u8g2.clearBuffer();
  RiiWynch::UI::drawFrame(u8g2);
  u8g2.setFont(u8g2_font_helvB18_tf); // Medium font for mode labels
  int16_t width = u8g2.getStrWidth(_modeDisplayText);
  u8g2.drawStr((128 - width) / 2, 44, _modeDisplayText);
  u8g2.sendBuffer();
}

void DisplayManager::updateModeDisplay() {
  if (!_modeDisplayActive) {
    return;
  }
  
  // Check if display time has elapsed
  if (millis() - _modeDisplayStartTime >= _modeDisplayDuration) {
    _modeDisplayActive = false;
  }
}

bool DisplayManager::isModeDisplayActive() {
  return _modeDisplayActive;
}

// ─────────────────────────────────────────
//             STOP SCREEN
// ─────────────────────────────────────────
void DisplayManager::drawStopScreen(int percentage, float rssi) {
  // If a mode label is currently shown, do not override it
  if (_modeDisplayActive) {
    return;
  }

  u8g2.clearBuffer();
  RiiWynch::UI::drawFrame(u8g2);

  // Title (large)
  const char* title = "STOP";
  u8g2.setFont(u8g2_font_logisoso28_tf);
  int16_t titleWidth = u8g2.getStrWidth(title);
  u8g2.drawStr((128 - titleWidth) / 2, 46, title);

  // Top info bar
  u8g2.setFont(u8g2_font_6x10_tf);

  // Percentage small left
  char pctBuf[8];
  sprintf(pctBuf, "%d%%", percentage);
  u8g2.drawStr(6, 13, pctBuf);

  // RSSI center
  char rssiBuf[10];
  sprintf(rssiBuf, "%.0fdBm", rssi);
  int16_t rssiWidth = u8g2.getStrWidth(rssiBuf);
  int16_t rssiX = (128 - rssiWidth - 20) / 2;
  u8g2.drawStr(rssiX, 13, rssiBuf);
  RiiWynch::UI::drawSignalStrength(u8g2, rssiX + rssiWidth + 5, 13, rssi);

  u8g2.sendBuffer();
}
