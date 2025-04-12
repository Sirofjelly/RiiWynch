#include <Arduino.h>

const int START_RELAY_PIN = 41;
const int STOP_RELAY_PIN  = 42;

unsigned long stopCooldownStart = 0;
bool stopCooldownActive = false;
unsigned long stopCooldownDuration = 10000;

void setupRelays() {
  pinMode(START_RELAY_PIN, OUTPUT);
  pinMode(STOP_RELAY_PIN, OUTPUT);

  digitalWrite(START_RELAY_PIN, HIGH);
  digitalWrite(STOP_RELAY_PIN, LOW);
}

void updateRelays(bool startPressed, bool stopPressed) {
  if (stopPressed) {
    digitalWrite(STOP_RELAY_PIN, HIGH);
    stopCooldownStart = millis();
    stopCooldownActive = true;
  } else if (stopCooldownActive) {
    if (millis() - stopCooldownStart >= stopCooldownDuration) {
      stopCooldownActive = false;
      digitalWrite(STOP_RELAY_PIN, LOW);
    } else {
      digitalWrite(STOP_RELAY_PIN, HIGH);
    }
  } else {
    digitalWrite(STOP_RELAY_PIN, LOW);
  }
}

bool isStopRelayInCooldown() {
  return stopCooldownActive;
}