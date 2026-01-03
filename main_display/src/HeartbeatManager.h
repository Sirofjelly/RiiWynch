#ifndef HEARTBEAT_MANAGER_H
#define HEARTBEAT_MANAGER_H

#include <FreeRTOS.h>
#include <semphr.h>
#include "StateManager.h"
#include "DisplayManager.h"

class ProfileManager; // Forward declaration

class HeartbeatManager {
public:
    HeartbeatManager(StateManager& stateMgr, DisplayManager& displayMgr);
    
    bool begin();
    void update();
    void onHeartbeatReceived();
    bool isRemoteConnected() const;
    void setProfileManager(ProfileManager* profMgr);
    SemaphoreHandle_t getMutex() { return heartbeatMutex; }
    
private:
    // References to other managers
    StateManager& state;
    DisplayManager& display;
    ProfileManager* profileManager;
    
    // Connection state
    unsigned long lastHeartbeatTime;
    bool remoteConnected;
    static const unsigned long HEARTBEAT_TIMEOUT = 2000; // ms
    static const unsigned long HEARTBEAT_HYSTERESIS = 200; // ms - extra time before declaring disconnected
    static const uint8_t TIMEOUT_CONFIRM_COUNT = 2; // Require N consecutive timeout checks before emergency stop
    uint8_t consecutiveTimeouts; // Counter for consecutive timeout detections

    // Emergency stop recovery state
    bool inEmergencyStop;
    unsigned long emergencyStopStartTime;
    static const unsigned long RECOVERY_TIMEOUT = 500; // 3 seconds as requested
    
    // FreeRTOS task
    TaskHandle_t taskHandle;
    SemaphoreHandle_t heartbeatMutex;
    
    // Task function
    static void heartbeatTask(void* parameter);
    void checkTimeout();
    void executeEmergencyStop();
    void handleReconnection();
    
    // Static instance for task access
    static HeartbeatManager* instance;
};

#endif // HEARTBEAT_MANAGER_H 