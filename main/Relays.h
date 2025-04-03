#pragma once

void setupRelays();
void updateRelays(bool startButtonState, bool stopButtonState);
bool isStopRelayInCooldown();