#include "HeartbeatManager.h"
#include "ProfileManager.h"
#include <Arduino.h>

// Static member definitions
HeartbeatManager* HeartbeatManager::instance = nullptr;

HeartbeatManager::HeartbeatManager(StateManager& stateMgr, DisplayManager& displayMgr)
    : state(stateMgr), display(displayMgr), profileManager(nullptr), remoteConnected(false),
      consecutiveTimeouts(0), inEmergencyStop(false), emergencyStopStartTime(0), taskHandle(NULL) {
    instance = this;
    lastHeartbeatTime = millis(); // Initialize to current time to avoid immediate timeout
}

bool HeartbeatManager::begin() {
    // Create mutex for heartbeat monitoring
    heartbeatMutex = xSemaphoreCreateMutex();
    if (heartbeatMutex == NULL) {
        Serial.println("Failed to create heartbeat mutex!");
        return false;
    }

    // Create heartbeat monitoring task on core 0 (core 1 is used by Arduino loop by default)
    xTaskCreatePinnedToCore(
        heartbeatTask,        // Task function
        "HeartbeatMonitor",   // Task name
        2048,                 // Stack size (bytes)
        NULL,                 // Task parameter
        2,                    // Priority (balanced priority for monitoring)
        &taskHandle,          // Task handle
        0                     // Core ID (0 or 1)
    );
    
    if (taskHandle == NULL) {
        Serial.println("Failed to create heartbeat monitor task!");
        return false;
    }
    
    Serial.println("Heartbeat monitor task created successfully on core 0");
    return true;
}

void HeartbeatManager::setProfileManager(ProfileManager* profMgr) {
    profileManager = profMgr;
}

void HeartbeatManager::update() {
    // Main loop can call this for any additional heartbeat logic if needed
    // Most work is done in the dedicated FreeRTOS task
}

void HeartbeatManager::onHeartbeatReceived() {
    // Take mutex before accessing shared heartbeat variables
    if (xSemaphoreTake(heartbeatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lastHeartbeatTime = millis();
        consecutiveTimeouts = 0; // Reset timeout counter on valid heartbeat

        // Check if we're recovering from an emergency stop
        if (!remoteConnected && inEmergencyStop) {
            Serial.println("[HBT Monitor] Remote reconnected! Starting recovery sequence.");
            handleReconnection();
        }

        remoteConnected = true;
        xSemaphoreGive(heartbeatMutex);
    }
}

bool HeartbeatManager::isRemoteConnected() const {
    return remoteConnected;
}

void HeartbeatManager::heartbeatTask(void* parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t checkPeriod = pdMS_TO_TICKS(100); // Check every 100ms for responsiveness
    
    for (;;) {
        // Wait for the next check time
        vTaskDelayUntil(&lastWakeTime, checkPeriod);
        
        if (instance) {
            instance->checkTimeout();
        }
    }
}

void HeartbeatManager::checkTimeout() {
    // Take mutex before accessing shared heartbeat variables
    if (xSemaphoreTake(heartbeatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        unsigned long currentTime = millis();
        // Rollover-safe elapsed time calculation (unsigned subtraction handles wrap correctly)
        unsigned long elapsed = currentTime - lastHeartbeatTime;

        // Check for heartbeat timeout with hysteresis (connection loss)
        // Use timeout + hysteresis and require consecutive confirmations to avoid oscillation
        if (remoteConnected && (elapsed > HEARTBEAT_TIMEOUT + HEARTBEAT_HYSTERESIS)) {
            consecutiveTimeouts++;
            if (consecutiveTimeouts >= TIMEOUT_CONFIRM_COUNT) {
                Serial.printf("[HBT Monitor] Remote connection lost! (%lu ms since last heartbeat, %d confirms)\n",
                             elapsed, consecutiveTimeouts);
                executeEmergencyStop();
                consecutiveTimeouts = 0; // Reset after triggering
            } else {
                Serial.printf("[HBT Monitor] Timeout detected (%lu ms), awaiting confirmation (%d/%d)\n",
                             elapsed, consecutiveTimeouts, TIMEOUT_CONFIRM_COUNT);
            }
        } else if (remoteConnected && elapsed <= HEARTBEAT_TIMEOUT) {
            // Connection is healthy, reset counter
            consecutiveTimeouts = 0;
        }

        // Check for recovery timeout (auto-exit after reconnection)
        // Rollover-safe calculation
        unsigned long recoveryElapsed = currentTime - emergencyStopStartTime;
        if (inEmergencyStop && remoteConnected && (recoveryElapsed >= RECOVERY_TIMEOUT)) {
            Serial.println("[HBT Monitor] Recovery timeout reached, exiting emergency stop.");
            state.exitStop();
            inEmergencyStop = false;
        }

        // Release mutex
        xSemaphoreGive(heartbeatMutex);
    }
}

void HeartbeatManager::executeEmergencyStop() {
    // Key safety action - stop the system immediately using timeout mechanism
    state.stopWithTimeout(RECOVERY_TIMEOUT); // Use timeout instead of direct setState
    
    // Update display to show connection lost
    display.updateText("Lost");
    
    // Update connection status and emergency stop tracking
    remoteConnected = false;
    inEmergencyStop = true;
    emergencyStopStartTime = millis();
    
    Serial.println("[HBT Monitor] Emergency stop executed with recovery timeout");
}

void HeartbeatManager::handleReconnection() {
    // When remote reconnects, start the recovery timer
    // The system will automatically exit STOPPED state after RECOVERY_TIMEOUT
    emergencyStopStartTime = millis();
    Serial.printf("[HBT Monitor] Recovery timer started, will auto-exit stop in %lu ms\n", RECOVERY_TIMEOUT);
} 