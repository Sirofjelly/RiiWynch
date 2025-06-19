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

void DisplayManager::update(int percentage, float rssi, const char* mode, bool isConnected) {
  // Don't update if mode display is active
  if (_modeDisplayActive) {
    return;
  }
  
  u8g2.clearBuffer();
  RiiWynch::UI::drawFrame(u8g2);

  // ─── Top status bar ──────────────────────────────────────────────
  u8g2.setFont(u8g2_font_6x10_tf);

  // Mode name (small) on the left - replaces percentage
  u8g2.drawStr(6, 13, mode);

  // RSSI (small) on the right side
  if (isConnected) {
    char rssiBuf[10];
    snprintf(rssiBuf, sizeof(rssiBuf), "%.0fdBm", rssi);
    int16_t rssiWidth = u8g2.getStrWidth(rssiBuf);
    u8g2.drawStr(128 - rssiWidth - 6, 13, rssiBuf);
  } else {
    // Show disconnected icon instead of RSSI when not connected
    const char* noRemoteText = "DISCONNECTED";
    int16_t noRemoteWidth = u8g2.getStrWidth(noRemoteText);
    u8g2.drawStr(128 - noRemoteWidth - 6, 13, noRemoteText);
  }

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
void DisplayManager::drawStopScreen(int percentage, float rssi, const char* mode, bool isConnected) {
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

  // Mode name small left - replaces percentage
  u8g2.drawStr(6, 13, mode);

  // RSSI on the right side
  if (isConnected) {
    char rssiBuf[10];
    sprintf(rssiBuf, "%.0fdBm", rssi);
    int16_t rssiWidth = u8g2.getStrWidth(rssiBuf);
    u8g2.drawStr(128 - rssiWidth - 6, 13, rssiBuf);
  } else {
    // Show disconnected icon instead of RSSI when not connected
    const char* noRemote = "No Remote";
    int16_t noRemoteWidth = u8g2.getStrWidth(noRemote);
    u8g2.drawStr(128 - noRemoteWidth - 6, 13, noRemote);
  }

  u8g2.sendBuffer();
}
