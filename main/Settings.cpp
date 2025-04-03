#include <EEPROM.h>
#include "Servos.h"
#include "Relays.h"
#include "StartupManager.h"

const int EEPROM_SIZE = 64;
const byte SETTINGS_MAGIC = 0x42;

void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) == SETTINGS_MAGIC) {
    starterRelayTime = EEPROM.read(1) << 8 | EEPROM.read(2);
    rampUpDuration   = EEPROM.read(3) << 8 | EEPROM.read(4);
    rampDownDuration = EEPROM.read(5) << 8 | EEPROM.read(6);
    gasIdleAngle     = EEPROM.read(7);
    gasMaxAngle      = EEPROM.read(8);
    chokeAngle       = EEPROM.read(9);
    brakeAngle       = EEPROM.read(10);
    stopCooldownDuration = EEPROM.read(11) << 8 | EEPROM.read(12);
  }
}

void saveSettings() {
  EEPROM.write(0, SETTINGS_MAGIC);
  EEPROM.write(1, (starterRelayTime >> 8) & 0xFF);
  EEPROM.write(2, starterRelayTime & 0xFF);
  EEPROM.write(3, (rampUpDuration >> 8) & 0xFF);
  EEPROM.write(4, rampUpDuration & 0xFF);
  EEPROM.write(5, (rampDownDuration >> 8) & 0xFF);
  EEPROM.write(6, rampDownDuration & 0xFF);
  EEPROM.write(7, gasIdleAngle);
  EEPROM.write(8, gasMaxAngle);
  EEPROM.write(9, chokeAngle);
  EEPROM.write(10, brakeAngle);
  EEPROM.write(11, (stopCooldownDuration >> 8) & 0xFF);
  EEPROM.write(12, stopCooldownDuration & 0xFF);
  EEPROM.commit();
}
