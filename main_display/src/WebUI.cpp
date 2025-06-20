#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "Settings.h"
#include "StateManager.h"
#include "DisplayManager.h"
#include "StartupManager.h"
#include "LoRaManager.h"
#include "ProfileManager.h"

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

extern DisplayManager display;

extern unsigned long totalStarts;
extern unsigned long totalRuntimeSeconds;

// Global LoRa settings
extern float loraFrequency;
extern int loraPower;
extern int loraSpreadingFactor;
extern int loraCodingRate;
extern float loraBandwidth;

// Forward declarations
extern StateManager& getGlobalStateManager();
extern class LoRaManager& getGlobalLoRaManager();
extern class ProfileManager& getGlobalProfileManager();

void handleLoraPage() {
  loadGlobalSettings(); // Load global LoRa settings

  String html_content = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>RiiWynch LoRa Settings</title>
  <style>
    body { background-color: #111; color: #00ffff; font-family: sans-serif; text-align: center; padding: 36px; margin-top: 100px; }
    .navbar { background-color: #222; overflow: hidden; position: fixed; top: 0; width: 100%; left: 0; z-index: 1000; }
    .navbar a { float: left; display: block; color: #00ffff; text-align: center; padding: 14px 16px; text-decoration: none; font-size: 1.8em; }
    .navbar a:hover { background-color: #ddd; color: black; }
    .navbar a.active { background-color: #ff00ff; color: white; }
    h2 { font-size: 3.3em; margin-bottom: 35px; }
    h3 { font-size: 2.5em; margin: 35px 0 20px 0; color: #ff00ff; border-bottom: 2px solid #ff00ff; padding-bottom: 10px; }
    .status-message { background-color: rgba(0, 255, 0, 0.2); color: #00ff00; padding: 18px; margin: 24px auto; border-radius: 13px; border: 2.75px solid #00ff00; max-width: 90%; font-size: 2em; display: none; }
    .form-row { display: flex; align-items: center; justify-content: flex-start; margin: 18px auto; max-width: 900px; }
    label { display: inline-block; width: 500px; text-align: right; margin-right: 40px; font-size: 2em; vertical-align: middle; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    input[type="text"] { width: 5ch; min-width: 5ch; max-width: 5ch; padding: 11px; background: transparent; border: 2.75px solid #ff00ff; color: #00ffff; font-family: sans-serif; font-size: 2em; border-radius: 13px; text-align: center; margin-left: 0; }
    .button { font-size: 2em; width: 260px; min-width: 10ch; max-width: 20ch; padding: 11px; border: 2.75px solid #ff00ff; color: #00ffff; background: transparent; font-family: sans-serif; border-radius: 13px; margin: 10px 0 26px 0; cursor: pointer; display: inline-block; white-space: nowrap; }
    .footer { font-size: 1.2em; color: #888; text-align: center; margin-top: 50px; }
  </style>
</head>
<body>
  <div class="navbar">
    <a href="/">Settings</a>
    <a href="/lora" class="active">LoRa</a>
    <a href="/stats">Stats</a>
  </div>
  <div id="statusMessage" class="status-message"></div>
  <h3>Global LoRa Settings</h3>
  <form id="loraSettingsForm" onsubmit="return false;">
)rawliteral";

  // Add LoRa settings form fields
  html_content += "<div class=\"form-row\"><label>Frequency (MHz):</label><input name=\"loraFrequency\" type=\"text\" value=\"" + String(loraFrequency, 1) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Power (dBm):</label><input name=\"loraPower\" type=\"text\" value=\"" + String(loraPower) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Spreading Factor:</label><input name=\"loraSpreadingFactor\" type=\"text\" value=\"" + String(loraSpreadingFactor) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Coding Rate:</label><input name=\"loraCodingRate\" type=\"text\" value=\"" + String(loraCodingRate) + "\"></div>";
  html_content += "<div class=\"form-row\"><label>Bandwidth (kHz):</label><input name=\"loraBandwidth\" type=\"text\" value=\"" + String(loraBandwidth, 1) + "\"></div>";
  
  html_content += R"rawliteral(
    <button type="button" class="button" onclick="applyLoRaSettings()">Apply LoRa</button>
    <button type="button" class="button" onclick="saveLoRaSettings()">Save LoRa</button>
  </form>

  <div class="footer">Gmacht mit &lt;3 vom Silvan</div>

  <script>
    function showStatusMessage(message, isSuccess = true) {
      const statusElem = document.getElementById('statusMessage');
      statusElem.style.display = 'block';
      statusElem.style.backgroundColor = isSuccess ? 'rgba(0, 255, 0, 0.2)' : 'rgba(255, 0, 0, 0.2)';
      statusElem.style.borderColor = isSuccess ? '#00ff00' : '#ff0000';
      statusElem.style.color = isSuccess ? '#00ff00' : '#ff0000';
      statusElem.innerHTML = message;
      setTimeout(() => { statusElem.style.display = 'none'; }, 5000);
    }
    function applyLoRaSettings() {
      const formData = new FormData(document.getElementById('loraSettingsForm'));
      fetch('/setLora', { method: 'POST', body: formData })
      .then(response => response.text())
      .then(data => { showStatusMessage('LoRa settings applied successfully!'); })
      .catch(error => { showStatusMessage('Error applying LoRa settings: ' + error, false); });
    }
    function saveLoRaSettings() {
      const form = document.getElementById('loraSettingsForm');
      const formData = new FormData(form);
      fetch('/saveLora', { method: 'POST', body: formData })
      .then(response => response.json())
      .then(data => { if(data.success) { showStatusMessage('LoRa settings saved successfully'); } else { showStatusMessage('Error saving LoRa settings', false); } })
      .catch(error => { showStatusMessage('Error saving LoRa settings: ' + error, false); });
      return false;
    }
  </script>
</body>
</html>)rawliteral";

  server.send(200, "text/html", html_content);
}

void handleRoot() {
  // Get current state from ProfileManager instead of globals
  ProfileManager& profileMgr = getGlobalProfileManager();
  loadSettingsForProfile(profileMgr.getCurrentProfile()); // Load current profile's values
  loadGlobalSettings(); // Load global LoRa settings

  String html_content = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>RiiWynch Settings</title>
  <style>
    body { background-color: #111; color: #00ffff; font-family: sans-serif; text-align: center; padding: 36px; margin-top: 100px; }
    .navbar { background-color: #222; overflow: hidden; position: fixed; top: 0; width: 100%; left: 0; z-index: 1000; }
    .navbar a { float: left; display: block; color: #00ffff; text-align: center; padding: 14px 16px; text-decoration: none; font-size: 1.8em; }
    .navbar a:hover { background-color: #ddd; color: black; }
    .navbar a.active { background-color: #ff00ff; color: white; }
    h2 { font-size: 3.3em; margin-bottom: 35px; }
    h3 { font-size: 2.5em; margin: 35px 0 20px 0; color: #ff00ff; border-bottom: 2px solid #ff00ff; padding-bottom: 10px; }
    .status-message { background-color: rgba(0, 255, 0, 0.2); color: #00ff00; padding: 18px; margin: 24px auto; border-radius: 13px; border: 2.75px solid #00ff00; max-width: 90%; font-size: 2em; display: none; }
    .form-row { display: flex; align-items: center; justify-content: flex-start; margin: 18px auto; max-width: 900px; }
    label { display: inline-block; width: 500px; text-align: right; margin-right: 40px; font-size: 2em; vertical-align: middle; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    input[type="text"] { width: 5ch; min-width: 5ch; max-width: 5ch; padding: 11px; background: transparent; border: 2.75px solid #ff00ff; color: #00ffff; font-family: sans-serif; font-size: 2em; border-radius: 13px; text-align: center; margin-left: 0; }
    .button { font-size: 2em; width: 260px; min-width: 10ch; max-width: 20ch; padding: 11px; border: 2.75px solid #ff00ff; color: #00ffff; background: transparent; font-family: sans-serif; border-radius: 13px; margin: 10px 0 26px 0; cursor: pointer; display: inline-block; white-space: nowrap; }
    .mode-row { font-size: 2em; margin: 26px auto 0 auto; display: flex; justify-content: center; align-items: center; gap: 16px; }
    .mode-btn-row { display: flex; justify-content: center; margin: 10px auto 26px auto; }
    #profileInput { width: 260px; min-width: 10ch; max-width: 20ch; font-size: 2em; }
    .section-separator { margin: 40px 0; }
    .footer { font-size: 1.2em; color: #888; text-align: center; margin-top: 50px; }
  </style>
</head>
<body>
  <div class="navbar">
    <a href="/" class="active">Settings</a>
    <a href="/lora">LoRa</a>
    <a href="/stats">Stats</a>
  </div>
  <h2>RiiWynch Engine Settings</h2>
  <div id="statusMessage" class="status-message"></div>
  <div class="mode-row"><input type="text" id="profileInput" value=")rawliteral";
  
  // Use ProfileManager to get current mode name
  html_content += profileMgr.getCurrentModeName();
  
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

  <div class="footer">Gmacht mit &lt;3 vom Silvan</div>

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
  
  // Explicitly save to the current profile using ProfileManager
  ProfileManager& profileMgr = getGlobalProfileManager();
  int currentProfileIndex = profileMgr.getCurrentProfile();
  Serial.printf("🔵 Now saving to profile %d...\n", currentProfileIndex + 1);
  saveSettingsForProfile(currentProfileIndex);
  
  // Force a hard delay to ensure EEPROM write completes
  delay(100);
  
  // Verify the values were actually saved by reading them back
  unsigned long tempStarterTime = starterRelayTime;
  int tempGasIdle = gasIdleAngle;
  
  // Re-load from EEPROM to verify
  loadSettingsForProfile(currentProfileIndex);
  
  Serial.printf("✅ Verification - Starter: %lu (expected %lu), Gas Idle: %d (expected %d)\n", 
                starterRelayTime, tempStarterTime, gasIdleAngle, tempGasIdle);
  
  // Return a JSON response with success status for proper client handling
  server.send(200, "application/json", "{\"success\":true}");
}

void handleToggleManual() {
  if (server.hasArg("state")) {
    bool newManualMode = server.arg("state") == "1";
    ProfileManager& profileMgr = getGlobalProfileManager();
    profileMgr.setManualMode(newManualMode);
    
    // Reset state machine to prevent ramping
    getGlobalStateManager().setTargetPercentage(newManualMode ? 5 : 0);
  }
  server.send(200, "text/plain", "OK");
}

// Ensure the Web UI correctly reflects the current profile
void handleSwitchProfile() {
    ProfileManager& profileMgr = getGlobalProfileManager();
    profileMgr.cycleProfile(); // Use ProfileManager to cycle profiles

    // Prepare JSON response with all loaded settings
    String jsonResponse = "{";
    jsonResponse += "\"profile\":" + String(profileMgr.getCurrentProfile()) + ",";
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
    jsonResponse += "\"manualMode\":" + String(profileMgr.isManualMode() ? "true" : "false");
    jsonResponse += "}";

    server.send(200, "application/json", jsonResponse);
}

void handleGetMode() {
  ProfileManager& profileMgr = getGlobalProfileManager();
  String jsonResponse = "{";
  jsonResponse += "\"profile\":" + String(profileMgr.getCurrentProfile()) + ",";
  jsonResponse += "\"manualMode\":" + String(profileMgr.isManualMode() ? "true" : "false");
  jsonResponse += "}";
  server.send(200, "application/json", jsonResponse);
}

void handleSetLora() {
  Serial.println("📡 Applying temporary LoRa values (not saving to EEPROM):");
  
  // Parse LoRa values with validation
  if (server.hasArg("loraFrequency")) {
    float newFreq = server.arg("loraFrequency").toFloat();
    if (newFreq >= 863.0 && newFreq <= 870.0) {
      loraFrequency = newFreq;
      Serial.printf("  Frequency: %.1f MHz\n", loraFrequency);
    } else {
      Serial.println("  Invalid frequency (must be 863-870 MHz)");
    }
  }
  
  if (server.hasArg("loraPower")) {
    int newPower = server.arg("loraPower").toInt();
    if (newPower >= 2 && newPower <= 22) {
      loraPower = newPower;
      Serial.printf("  Power: %d dBm\n", loraPower);
    } else {
      Serial.println("  Invalid power (must be 2-22 dBm)");
    }
  }
  
  if (server.hasArg("loraSpreadingFactor")) {
    int newSF = server.arg("loraSpreadingFactor").toInt();
    if (newSF >= 7 && newSF <= 12) {
      loraSpreadingFactor = newSF;
      Serial.printf("  Spreading Factor: %d\n", loraSpreadingFactor);
    } else {
      Serial.println("  Invalid SF (must be 7-12)");
    }
  }
  
  if (server.hasArg("loraCodingRate")) {
    int newCR = server.arg("loraCodingRate").toInt();
    if (newCR >= 5 && newCR <= 8) {
      loraCodingRate = newCR;
      Serial.printf("  Coding Rate: %d\n", loraCodingRate);
    } else {
      Serial.println("  Invalid CR (must be 5-8)");
    }
  }
  
  if (server.hasArg("loraBandwidth")) {
    float newBW = server.arg("loraBandwidth").toFloat();
    // Validate against common bandwidth values
    if (newBW >= 7.8 && newBW <= 500.0) {
      loraBandwidth = newBW;
      Serial.printf("  Bandwidth: %.1f kHz\n", loraBandwidth);
    } else {
      Serial.println("  Invalid bandwidth (must be 7.8-500 kHz)");
    }
  }

  // Restart LoRa with new settings
  Serial.println("📡 Restarting LoRa with new settings...");
  if (getGlobalLoRaManager().restart()) {
    Serial.println("✅ LoRa restarted successfully");
    
    // Send new settings to remote
    Serial.println("📡 Sending new LoRa settings to remote...");
    getGlobalLoRaManager().sendLoRaSettings();
    
    server.send(200, "text/plain", "OK");
  } else {
    Serial.println("❌ LoRa restart failed");
    server.send(500, "text/plain", "LoRa restart failed");
  }
}

void handleSaveLora() {
  Serial.println("💾 Saving LoRa values to EEPROM...");
  
  // First apply the values (same validation as handleSetLora)
  if (server.hasArg("loraFrequency")) {
    float newFreq = server.arg("loraFrequency").toFloat();
    if (newFreq >= 863.0 && newFreq <= 870.0) {
      loraFrequency = newFreq;
    }
  }
  
  if (server.hasArg("loraPower")) {
    int newPower = server.arg("loraPower").toInt();
    if (newPower >= 2 && newPower <= 22) {
      loraPower = newPower;
    }
  }
  
  if (server.hasArg("loraSpreadingFactor")) {
    int newSF = server.arg("loraSpreadingFactor").toInt();
    if (newSF >= 7 && newSF <= 12) {
      loraSpreadingFactor = newSF;
    }
  }
  
  if (server.hasArg("loraCodingRate")) {
    int newCR = server.arg("loraCodingRate").toInt();
    if (newCR >= 5 && newCR <= 8) {
      loraCodingRate = newCR;
    }
  }
  
  if (server.hasArg("loraBandwidth")) {
    float newBW = server.arg("loraBandwidth").toFloat();
    if (newBW >= 7.8 && newBW <= 500.0) {
      loraBandwidth = newBW;
    }
  }
  
  // Save to EEPROM
  saveGlobalSettings();
  
  // Restart LoRa with new settings
  Serial.println("📡 Restarting LoRa with saved settings...");
  bool restartSuccess = getGlobalLoRaManager().restart();
  
  if (restartSuccess) {
    // Send new settings to remote
    Serial.println("📡 Sending saved LoRa settings to remote...");
    getGlobalLoRaManager().sendLoRaSettings();
  }
  
  // Return JSON response
  String jsonResponse = "{";
  jsonResponse += "\"success\":" + String(restartSuccess ? "true" : "false");
  jsonResponse += "}";
  server.send(200, "application/json", jsonResponse);
}

void handleResetStats() {
  resetStats();
  server.send(200, "text/plain", "OK");
}

void handleStats() {
  unsigned long hours = totalRuntimeSeconds / 3600;
  unsigned long minutes = (totalRuntimeSeconds % 3600) / 60;
  unsigned long seconds = totalRuntimeSeconds % 60;

  String html_content = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>RiiWynch Statistics</title>
  <style>
    body { background-color: #111; color: #00ffff; font-family: sans-serif; text-align: center; padding: 36px; margin-top: 100px; }
    .navbar { background-color: #222; overflow: hidden; position: fixed; top: 0; width: 100%; left: 0; z-index: 1000; }
    .navbar a { float: left; display: block; color: #00ffff; text-align: center; padding: 14px 16px; text-decoration: none; font-size: 1.8em; }
    .navbar a:hover { background-color: #ddd; color: black; }
    .navbar a.active { background-color: #ff00ff; color: white; }
    h2 { font-size: 3.3em; margin-bottom: 35px; }
    .stat-item { font-size: 2em; margin: 20px; }
    .button { font-size: 2em; width: 260px; padding: 11px; border: 2.75px solid #ff00ff; color: #00ffff; background: transparent; font-family: sans-serif; border-radius: 13px; margin-top: 40px; cursor: pointer; text-decoration: none; display: inline-block; }
    .footer { font-size: 1.2em; color: #888; text-align: center; margin-top: 50px; }
  </style>
</head>
<body>
  <div class="navbar">
    <a href="/">Settings</a>
    <a href="/lora">LoRa</a>
    <a href="/stats" class="active">Stats</a>
  </div>
  <h2>Engine Statistics</h2>
  <div class="stat-item">Total Starts: <span id="totalStarts">)rawliteral";
  html_content += String(totalStarts);
  html_content += R"rawliteral(</span></div>
  <div class="stat-item">Total Runtime: <span id="totalRuntime">)rawliteral";
  html_content += String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
  html_content += R"rawliteral(</span></div>
  <button type="button" class="button" onclick="resetStats()">Reset Stats</button>
  <div class="footer">Gmacht mit &lt;3 vom Silvan</div>
  <script>
    function resetStats() {
      if (confirm('Are you sure you want to reset all statistics? This cannot be undone.')) {
        fetch('/resetStats', { method: 'POST' })
        .then(response => {
          if (response.ok) {
            // Reload the page to show the reset values
            location.reload();
          } else {
            alert('Error resetting statistics.');
          }
        });
      }
    }
  </script>
</body>
</html>)rawliteral";
  server.send(200, "text/html", html_content);
}

void setupWebUI() {
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/stats", handleStats);
  server.on("/resetStats", HTTP_POST, handleResetStats);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/save", HTTP_POST, handleSetDefault); // Change from "/set-default" to "/save"
  server.on("/toggleManual", handleToggleManual);
  server.on("/switchProfile", handleSwitchProfile);
  server.on("/getMode", handleGetMode); // New endpoint
  server.on("/setLora", HTTP_POST, handleSetLora); // New LoRa endpoints
  server.on("/saveLora", HTTP_POST, handleSaveLora);
  server.on("/lora", handleLoraPage);
  server.begin();
}

void handleWebUI() {
  server.handleClient();
}
