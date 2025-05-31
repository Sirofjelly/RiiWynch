#include "TaskManager.h"
#include "LoRaManager.h"
#include "HeartbeatManager.h"
#include <Arduino.h>

TaskManager::TaskManager()
    : loraManager(nullptr), heartbeatManager(nullptr),
      loraTaskHandle(NULL), heartbeatTaskHandle(NULL), tasksCreated(false) {
}

bool TaskManager::begin() {
    // Tasks are actually created by their respective managers
    // This manager just provides coordination and monitoring
    tasksCreated = true;
    return true;
}

void TaskManager::registerLoRaManager(LoRaManager* lora) {
    loraManager = lora;
}

void TaskManager::registerHeartbeatManager(HeartbeatManager* heartbeat) {
    heartbeatManager = heartbeat;
}

void TaskManager::printTaskStatus() {
    if (!tasksCreated) {
        Serial.println("[TaskManager] Tasks not yet created");
        return;
    }
    
    // Print task status information
    Serial.println("[TaskManager] Task Status:");
    
    // Get task list (this is a more advanced feature)
    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    Serial.printf("  Total tasks: %d\n", taskCount);
    
    // Print heap information
    Serial.printf("  Free heap: %d bytes\n", esp_get_free_heap_size());
    Serial.printf("  Minimum free heap: %d bytes\n", esp_get_minimum_free_heap_size());
    
    // Task-specific status
    if (loraManager) {
        Serial.println("  LoRa Manager: Active");
    }
    
    if (heartbeatManager) {
        Serial.printf("  Heartbeat Manager: Active (Remote %s)\n", 
                     heartbeatManager->isRemoteConnected() ? "Connected" : "Disconnected");
    }
} 