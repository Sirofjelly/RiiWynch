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
    
    // FreeRTOS task
    TaskHandle_t taskHandle;
    SemaphoreHandle_t heartbeatMutex;
    
    // Task function
    static void heartbeatTask(void* parameter);
    void checkTimeout();
    void executeEmergencyStop();
    
    // Static instance for task access
    static HeartbeatManager* instance;
};

#endif // HEARTBEAT_MANAGER_H 