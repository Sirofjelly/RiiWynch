#pragma once
#include <ESP32Servo.h>

void setupServos();
void updateServos(bool chokePressed, bool brakePressed);

extern int gasIdleAngle;
extern int gasMaxAngle;
extern Servo gasServo;
extern int chokeAngle;
extern int brakeAngle;