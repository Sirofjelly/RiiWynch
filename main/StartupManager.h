#pragma once

void setupStartup();
void updateStartup(bool startPressed, bool stopPressed);

extern unsigned long starterRelayTime;
extern unsigned long rampUpDuration;
extern unsigned long rampDownDuration;