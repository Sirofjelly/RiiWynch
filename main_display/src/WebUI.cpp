#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "Settings.h"
#include "StateManager.h"
#include "DisplayManager.h"
#include "StartupManager.h"

const char* ssid = "RiiWynch";
const char* password = "123456789";

WebServer server(80);

// External variables
extern unsigned long starterRelayTime;
extern int stage1SpeedPercentage;
extern unsigned long stage1Duration;
extern unsigned long stage2Duration;
extern unsigned long stage3Duration;
extern int gasIdleAngle;
extern int gasMaxAngle;
extern int chokeAngle;
extern int brakeAngle;
extern unsigned long stopCooldownDuration;
extern bool manualMode;
extern StateManager state;
extern int currentProfile; // Use the `extern` declaration from Settings.h
extern DisplayManager display;
extern const char* modeNames[4];

void handleRoot() {
  if (manualMode) currentProfile = 3;
  loadSettingsForProfile(currentProfile); // Always load current profile's values

  String html_content = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>RiiWynch Settings</title>
  <style>
    body { background-color: #111; color: #00ffff; font-family: sans-serif; text-align: center; padding: 36px; }
    h2 { font-size: 3.3em; margin-bottom: 35px; }
    .status-message { background-color: rgba(0, 255, 0, 0.2); color: #00ff00; padding: 18px; margin: 24px auto; border-radius: 13px; border: 2.75px solid #00ff00; max-width: 90%; font-size: 2em; display: none; }
    .form-row { display: flex; align-items: center; justify-content: flex-start; margin: 18px auto; max-width: 900px; }
    label { display: inline-block; width: 500px; text-align: right; margin-right: 40px; font-size: 2em; vertical-align: middle; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    input[type="text"] { width: 5ch; min-width: 5ch; max-width: 5ch; padding: 11px; background: transparent; border: 2.75px solid #ff00ff; color: #00ffff; font-family: sans-serif; font-size: 2em; border-radius: 13px; text-align: center; margin-left: 0; }
    .button { font-size: 2em; width: 260px; min-width: 10ch; max-width: 20ch; padding: 11px; border: 2.75px solid #ff00ff; color: #00ffff; background: transparent; font-family: sans-serif; border-radius: 13px; margin: 10px 0 26px 0; cursor: pointer; display: inline-block; white-space: nowrap; }
    .mode-row { font-size: 2em; margin: 26px auto 0 auto; display: flex; justify-content: center; align-items: center; gap: 16px; }
    .mode-btn-row { display: flex; justify-content: center; margin: 10px auto 26px auto; }
    #profileInput { width: 260px; min-width: 10ch; max-width: 20ch; font-size: 2em; }
  </style>
</head>
<body>
  <h2>RiiWynch Engine Settings</h2>
  <div id="statusMessage" class="status-message"></div>
  <div class="mode-row"><input type="text" id="profileInput" value=")rawliteral";
  
  html_content += modeNames[manualMode ? 3 : currentProfile];
  
  html_content += R"rawliteral(" readonly></div>
  <div class="mode-btn-row"><button type="button" class="button" onclick="switchProfile()">Change Mode</button></div>
  <form id="settingsForm" onsubmit="return false;">
)rawliteral";

  html_content += "<div class=\"form-row\"><label>Starter Relay Time (ms):</label><input name=\"starterRelayTime\" type=\"text\" value=\"" + String(starterRelayTime) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Stage 1 Speed (%) :</label><input name=\"stage1Speed\" type=\"text\" value=\"" + String(stage1SpeedPercentage) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Stage 1 Time (ms):</label><input name=\"stage1Duration\" type=\"text\" value=\"" + String(stage1Duration) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Stage 2 Time (ms):</label><input name=\"stage2Duration\" type=\"text\" value=\"" + String(stage2Duration) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Stage 3 Time (ms):</label><input name=\"stage3Duration\" type=\"text\" value=\"" + String(stage3Duration) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Gas Idle Angle (°):</label><input name=\"gasIdleAngle\" type=\"text\" value=\"" + String(gasIdleAngle) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Gas Max Angle (°):</label><input name=\"gasMaxAngle\" type=\"text\" value=\"" + String(gasMaxAngle) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Choke Angle (°):</label><input name=\"chokeAngle\" type=\"text\" value=\"" + String(chokeAngle) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Brake Angle (°):</label><input name=\"brakeAngle\" type=\"text\" value=\"" + String(brakeAngle) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Stop Cooldown (ms):</label><input name=\"stopCooldownDuration\" type=\"text\" value=\"" + String(stopCooldownDuration) + "\"></div>";
  
  html_content += R"rawliteral(
    <button type="button" class="button" onclick="applySettings()">Apply</button>
    <button type="button" class="button" onclick="saveSettings()">Save</button>
  </form>

  <script>
    const modeNames = ["SURF", "SKIM", "SMOOTH", "MANUAL"];
    function updateModeBox() {
      fetch('/getMode')
        .then(response => response.json())
        .then(data => {
          document.getElementById('profileInput').value = modeNames[data.manualMode ? 3 : data.profile];
        });
    }
    function showStatusMessage(message, isSuccess = true) {
      const statusElem = document.getElementById('statusMessage');
      statusElem.style.display = 'block';
      statusElem.style.backgroundColor = isSuccess ? 'rgba(0, 255, 0, 0.2)' : 'rgba(255, 0, 0, 0.2)';
      statusElem.style.borderColor = isSuccess ? '#00ff00' : '#ff0000';
      statusElem.style.color = isSuccess ? '#00ff00' : '#ff0000';
      statusElem.innerHTML = message;
      setTimeout(() => { statusElem.style.display = 'none'; }, 5000);
    }
    function applySettings() {
      const formData = new FormData(document.getElementById('settingsForm'));
      fetch('/set', { method: 'POST', body: formData })
      .then(response => response.text())
      .then(data => { showStatusMessage('Settings applied successfully!'); })
      .catch(error => { showStatusMessage('Error applying settings: ' + error, false); });
      updateModeBox();
    }
    function saveSettings() {
      const form = document.getElementById('settingsForm');
      const formData = new FormData(form);
      fetch('/save', { method: 'POST', body: formData })
      .then(response => response.json())
      .then(data => { if(data.success) { showStatusMessage('Settings saved successfully'); } else { showStatusMessage('Error saving settings', false); } })
      .catch(error => { showStatusMessage('Error saving settings: ' + error, false); });
      updateModeBox();
      return false;
    }
    function switchProfile() {
      fetch('/switchProfile')
        .then(response => response.json())
        .then(data => {
          updateModeBox();
          document.querySelector('input[name="starterRelayTime"]').value = data.starterTime;
          document.querySelector('input[name="stage1Duration"]').value = data.stage1Duration;
          document.querySelector('input[name="stage1Speed"]').value = data.stage1Speed;
          document.querySelector('input[name="stage2Duration"]').value = data.stage2Duration;
          document.querySelector('input[name="stage3Duration"]').value = data.stage3Duration;
          document.querySelector('input[name="gasIdleAngle"]').value = data.gasIdleAngle;
          document.querySelector('input[name="gasMaxAngle"]').value = data.gasMaxAngle;
          document.querySelector('input[name="chokeAngle"]').value = data.chokeAngle;
          document.querySelector('input[name="brakeAngle"]').value = data.brakeAngle;
          document.querySelector('input[name="stopCooldownDuration"]').value = data.stopCooldownDuration;
          showStatusMessage('Switched to Mode ' + modeNames[data.manualMode ? 3 : data.profile]);
        })
        .catch(error => { showStatusMessage('Error switching mode: ' + error, false); });
    }
    // On page load, sync mode box
    window.onload = updateModeBox;
  </script>
</body>
</html>)rawliteral";

  server.send(200, "text/html", html_content);
}

void handleSet() {
  Serial.println("🔧 Applying temporary values (not saving to EEPROM):");
  
  // Save old values for debugging
  unsigned long oldStarterTime = starterRelayTime;
  int oldGasIdle = gasIdleAngle;
  
  // Parse values with careful type conversion
  if (server.hasArg("starterRelayTime")) {
    starterRelayTime = server.arg("starterRelayTime").toInt();
    Serial.printf("  Starter Relay Time: %lu (was %lu)\n", starterRelayTime, oldStarterTime);
  }
  
  if (server.hasArg("stage1Duration")) {
    stage1Duration = server.arg("stage1Duration").toInt();
  }
  
  if (server.hasArg("stage1Speed")) {
    // Convert to float with bounds checking
    stage1SpeedPercentage = server.arg("stage1Speed").toInt();
  }
  
  if (server.hasArg("stage2Duration")) {
    stage2Duration = server.arg("stage2Duration").toInt();
  }
  
  if (server.hasArg("stage3Duration")) {
    stage3Duration = server.arg("stage3Duration").toInt();
  }
  
  if (server.hasArg("gasIdleAngle")) {
    gasIdleAngle = server.arg("gasIdleAngle").toInt();
    Serial.printf("  Gas Idle Angle: %d (was %d)\n", gasIdleAngle, oldGasIdle);
  }
  
  if (server.hasArg("gasMaxAngle")) {
    gasMaxAngle = server.arg("gasMaxAngle").toInt();
  }
  
  if (server.hasArg("chokeAngle")) {
    chokeAngle = server.arg("chokeAngle").toInt();
  }
  
  if (server.hasArg("brakeAngle")) {
    brakeAngle = server.arg("brakeAngle").toInt();
  }
  
  if (server.hasArg("stopCooldownDuration")) {
    stopCooldownDuration = server.arg("stopCooldownDuration").toInt();
  }

  // Send a simple success response for AJAX request
  server.send(200, "text/plain", "OK");
}

void handleSetDefault() {
  Serial.println("🔄 Saving values to profile...");
  
  // Save old values for debugging
  unsigned long oldStarterTime = starterRelayTime;
  int oldGasIdle = gasIdleAngle;
  
  // Parse values with careful type conversion and bounds checking
  if (server.hasArg("starterRelayTime")) {
    starterRelayTime = server.arg("starterRelayTime").toInt();
    Serial.printf("  Starter Relay Time: %lu (was %lu)\n", starterRelayTime, oldStarterTime);
  }
  
  if (server.hasArg("stage1Duration")) {
    stage1Duration = server.arg("stage1Duration").toInt();
  }
  
  if (server.hasArg("stage1Speed")) {
    // Convert to float with bounds checking
    stage1SpeedPercentage = server.arg("stage1Speed").toInt();
  }
  
  if (server.hasArg("stage2Duration")) {
    stage2Duration = server.arg("stage2Duration").toInt();
  }
  
  if (server.hasArg("stage3Duration")) {
    stage3Duration = server.arg("stage3Duration").toInt();
  }
  
  if (server.hasArg("gasIdleAngle")) {
    gasIdleAngle = server.arg("gasIdleAngle").toInt();
    Serial.printf("  Gas Idle Angle: %d (was %d)\n", gasIdleAngle, oldGasIdle);
  }
  
  if (server.hasArg("gasMaxAngle")) {
    gasMaxAngle = server.arg("gasMaxAngle").toInt();
  }
  
  if (server.hasArg("chokeAngle")) {
    chokeAngle = server.arg("chokeAngle").toInt();
  }
  
  if (server.hasArg("brakeAngle")) {
    brakeAngle = server.arg("brakeAngle").toInt();
  }
  
  if (server.hasArg("stopCooldownDuration")) {
    stopCooldownDuration = server.arg("stopCooldownDuration").toInt();
  }
  
  // Explicitly save to the current profile
  if (manualMode) currentProfile = 3;
  Serial.printf("🔵 Now saving to profile %d...\n", currentProfile + 1);
  saveSettingsForProfile(currentProfile);
  
  // Force a hard delay to ensure EEPROM write completes
  delay(100);
  
  // Verify the values were actually saved by reading them back
  unsigned long tempStarterTime = starterRelayTime;
  int tempGasIdle = gasIdleAngle;
  
  // Re-load from EEPROM to verify
  loadSettingsForProfile(currentProfile);
  
  Serial.printf("✅ Verification - Starter: %lu (expected %lu), Gas Idle: %d (expected %d)\n", 
                starterRelayTime, tempStarterTime, gasIdleAngle, tempGasIdle);
  
  // Return a JSON response with success status for proper client handling
  server.send(200, "application/json", "{\"success\":true}");
}

void handleToggleManual() {
  if (server.hasArg("state")) {
    manualMode = server.arg("state") == "1";
    if (manualMode) {
      state.setTargetPercentage(5);
      currentState = IDLE; // Reset state machine to prevent ramping
      display.startModeDisplay(modeNames[3], 1500);
    } else {
      currentState = IDLE;
      display.startModeDisplay(modeNames[currentProfile], 1500);
    }
  }
  server.send(200, "text/plain", "OK");
}

// Ensure the Web UI correctly reflects the current profile
void handleSwitchProfile() {
    saveSettingsForProfile(currentProfile);
    Serial.printf("💾 Saved settings to profile %d before switching\n", currentProfile + 1);

    // Cycle to the next profile (0-3)
    currentProfile = (currentProfile + 1) % 4;
    manualMode = (currentProfile == 3);
    loadSettingsForProfile(currentProfile);

    Serial.printf("🔄 Switching to profile %d\n", currentProfile + 1);
    display.startModeDisplay(modeNames[currentProfile], 1500);

    // Prepare JSON response with all loaded settings
    String jsonResponse = "{";
    jsonResponse += "\"profile\":" + String(currentProfile) + ","; // Correctly reflect the current profile
    jsonResponse += "\"starterTime\":" + String(starterRelayTime) + ",";
    jsonResponse += "\"stage1Duration\":" + String(stage1Duration) + ",";
    jsonResponse += "\"stage1Speed\":" + String(stage1SpeedPercentage) + ",";
    jsonResponse += "\"stage2Duration\":" + String(stage2Duration) + ",";
    jsonResponse += "\"stage3Duration\":" + String(stage3Duration) + ",";
    jsonResponse += "\"gasIdleAngle\":" + String(gasIdleAngle) + ",";
    jsonResponse += "\"gasMaxAngle\":" + String(gasMaxAngle) + ",";
    jsonResponse += "\"chokeAngle\":" + String(chokeAngle) + ",";
    jsonResponse += "\"brakeAngle\":" + String(brakeAngle) + ",";
    jsonResponse += "\"stopCooldownDuration\":" + String(stopCooldownDuration) + ",";
    jsonResponse += "\"manualMode\":" + String(manualMode ? "true" : "false");
    jsonResponse += "}";

    server.send(200, "application/json", jsonResponse);
}

void handleGetMode() {
  String jsonResponse = "{";
  jsonResponse += "\"profile\":" + String(currentProfile) + ",";
  jsonResponse += "\"manualMode\":" + String(manualMode ? "true" : "false");
  jsonResponse += "}";
  server.send(200, "application/json", jsonResponse);
}

void setupWebUI() {
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/save", HTTP_POST, handleSetDefault); // Change from "/set-default" to "/save"
  server.on("/toggleManual", handleToggleManual);
  server.on("/switchProfile", handleSwitchProfile);
  server.on("/getMode", handleGetMode); // New endpoint
  server.begin();
}

void handleWebUI() {
  server.handleClient();
}
