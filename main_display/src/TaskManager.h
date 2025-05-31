#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <FreeRTOS.h>
#include <semphr.h>

class LoRaManager;
class HeartbeatManager;

class TaskManager {
public:
    TaskManager();
    
    bool begin();
    void registerLoRaManager(LoRaManager* lora);
    void registerHeartbeatManager(HeartbeatManager* heartbeat);
    
    // Task monitoring (future enhancement)
    void printTaskStatus();
    
private:
    // Manager references
    LoRaManager* loraManager;
    HeartbeatManager* heartbeatManager;
    
    // Task handles (if we need direct access)
    TaskHandle_t loraTaskHandle;
    TaskHandle_t heartbeatTaskHandle;
    
    bool tasksCreated;
};

#endif // TASK_MANAGER_H 