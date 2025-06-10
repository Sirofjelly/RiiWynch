#include "HeartbeatManager.h"
#include "ProfileManager.h"
#include <Arduino.h>

// Static member definitions
HeartbeatManager* HeartbeatManager::instance = nullptr;

HeartbeatManager::HeartbeatManager(StateManager& stateMgr, DisplayManager& displayMgr)
    : state(stateMgr), display(displayMgr), profileManager(nullptr), remoteConnected(false), taskHandle(NULL) {
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
        3,                    // Priority (higher than default for safety)
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
        if (!remoteConnected) {
            Serial.println("Remote (re)connected.");
            state.setEmergencyStop(false); // Clear emergency stop on reconnect
            // Show current mode when remote reconnects
            if (profileManager) {
                profileManager->showModeOnReconnect();
            }
        }
        lastHeartbeatTime = millis();
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
        // Check for heartbeat timeout
        if (remoteConnected && (millis() - lastHeartbeatTime > HEARTBEAT_TIMEOUT)) {
            Serial.println("[HBT Monitor] Remote connection lost! Initiating STOP sequence.");
            executeEmergencyStop();
        }
        
        // Release mutex
        xSemaphoreGive(heartbeatMutex);
    }
}

void HeartbeatManager::executeEmergencyStop() {
    // Key safety action - stop the system immediately
    state.setTargetPercentage(0);
    state.setEmergencyStop(true);
    
    // Update display to show connection lost
    display.updateText("Lost");
    
    // Update connection status
    remoteConnected = false;
    
    Serial.println("[HBT Monitor] Emergency stop executed");
} 