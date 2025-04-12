#include <EEPROM.h>
#include "Servos.h"
#include "StartupManager.h"
#include "Relays.h"

extern bool manualMode;

void saveSettings();  // ensure this is declared before loadSettings()

void loadSettings() {
  EEPROM.begin(64);

  if (EEPROM.read(0) != 0x42) {
    Serial.println("EEPROM uninitialized — loading default settings.");

    starterRelayTime     = 1500;
    rampUpDuration       = 2000;
    rampUpExponent       = 3.00;
    rampDownDuration     = 1000;
    gasIdleAngle         = 15;
    gasMaxAngle          = 65;
    chokeAngle           = 55;
    brakeAngle           = 55;
    stopCooldownDuration = 3000;
    manualMode           = false;

    saveSettings();  // Save to EEPROM
  } else {
    starterRelayTime     = EEPROM.read(1) << 8 | EEPROM.read(2);
    rampUpDuration       = EEPROM.read(3) << 8 | EEPROM.read(4);
    rampUpExponent       = EEPROM.read(5) / 10.0;
    rampDownDuration     = EEPROM.read(6) << 8 | EEPROM.read(7);
    gasIdleAngle         = EEPROM.read(8);
    gasMaxAngle          = EEPROM.read(9);
    chokeAngle           = EEPROM.read(10);
    brakeAngle           = EEPROM.read(11);
    stopCooldownDuration = EEPROM.read(12) << 8 | EEPROM.read(13);
    manualMode           = EEPROM.read(14) == 1;

    Serial.println("Loaded settings from EEPROM:");
  }

  // ✅ Print settings whether from default or EEPROM
  Serial.printf("Starter Relay Time:     %lu ms\n", starterRelayTime);
  Serial.printf("Ramp Up Duration:       %lu ms\n", rampUpDuration);
  Serial.printf("Ramp-Up Exponent:       %.2f\n", rampUpExponent);
  Serial.printf("Ramp Down Duration:     %lu ms\n", rampDownDuration);
  Serial.printf("Gas Idle Angle:         %d°\n", gasIdleAngle);
  Serial.printf("Gas Max Angle:          %d°\n", gasMaxAngle);
  Serial.printf("Choke Angle:            %d°\n", chokeAngle);
  Serial.printf("Brake Angle:            %d°\n", brakeAngle);
  Serial.printf("Stop Cooldown Duration: %lu ms\n", stopCooldownDuration);
  Serial.printf("Manual Mode:            %s\n", manualMode ? "ON" : "OFF");
}

void saveSettings() {
  EEPROM.write(0, 0x42);  // Signature byte
  EEPROM.write(1, (starterRelayTime >> 8) & 0xFF);
  EEPROM.write(2, starterRelayTime & 0xFF);
  EEPROM.write(3, (rampUpDuration >> 8) & 0xFF);
  EEPROM.write(4, rampUpDuration & 0xFF);
  EEPROM.write(5, (int)(rampUpExponent * 10));
  EEPROM.write(6, (rampDownDuration >> 8) & 0xFF);
  EEPROM.write(7, rampDownDuration & 0xFF);
  EEPROM.write(8, gasIdleAngle);
  EEPROM.write(9, gasMaxAngle);
  EEPROM.write(10, chokeAngle);
  EEPROM.write(11, brakeAngle);
  EEPROM.write(12, (stopCooldownDuration >> 8) & 0xFF);
  EEPROM.write(13, stopCooldownDuration & 0xFF);
  EEPROM.write(14, manualMode ? 1 : 0);
  EEPROM.commit();
}
