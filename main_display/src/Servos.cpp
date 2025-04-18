#include <ESP32Servo.h>
#include <Arduino.h>
#include "Settings.h"

const int CHOKE_SERVO_PIN = 36;
const int BRAKE_SERVO_PIN = 47;
const int GAS_SERVO_PIN   = 35;

Servo chokeServo;
Servo brakeServo;
Servo gasServo;

void setupServos() {
  chokeServo.attach(CHOKE_SERVO_PIN);
  brakeServo.attach(BRAKE_SERVO_PIN);
  gasServo.attach(GAS_SERVO_PIN);

  chokeServo.write(0);
  brakeServo.write(0);
  gasServo.write(0);
}

void updateServos(bool chokePressed, bool brakePressed) {
  chokeServo.write(chokePressed ? (180 - chokeAngle) : 180);
  brakeServo.write(brakePressed ? brakeAngle : 0);
}