#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "Settings.h"
#include "StateManager.h"

const char* ssid = "RiiWynch";
const char* password = "912345678";

WebServer server(80);

// External variables
extern unsigned long starterRelayTime;
extern unsigned long rampUpDuration;
extern float rampUpExponent;
extern unsigned long rampDownDuration;
extern int gasIdleAngle;
extern int gasMaxAngle;
extern int chokeAngle;
extern int brakeAngle;
extern unsigned long stopCooldownDuration;
extern bool manualMode;
extern StateManager state;
extern int currentProfile; // Use the `extern` declaration from Settings.h

const int totalProfiles = 3; // Example: 3 profiles

void handleRoot() {
  if (manualMode) currentProfile = 3;
  loadSettingsForProfile(currentProfile); // Always load current profile's values before rendering
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  const char* modeNames[4] = {"SURF", "SKIM", "SMOOTH", "MANUAL"};
  String html_part1 = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>RiiWynch Settings</title>
  <link href="https://fonts.googleapis.com/css2?family=Orbitron&display=swap" rel="stylesheet">
  <style>
    body { background-color: #111; color: #00ffff; font-family: 'Orbitron', sans-serif; text-align: center; padding: 36px; }
    h2 { font-size: 3.3em; margin-bottom: 35px; }
    .status-message { background-color: rgba(0, 255, 0, 0.2); color: #00ff00; padding: 18px; margin: 24px auto; border-radius: 13px; border: 2.75px solid #00ff00; max-width: 90%; font-size: 2em; display: none; }
    .form-row { display: flex; align-items: center; justify-content: flex-start; margin: 18px auto; max-width: 900px; }
    label { display: inline-block; width: 500px; text-align: right; margin-right: 40px; font-size: 2em; vertical-align: middle; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    input[type="text"] { width: 5ch; min-width: 5ch; max-width: 5ch; padding: 11px; background: transparent; border: 2.75px solid #ff00ff; color: #00ffff; font-family: 'Orbitron', sans-serif; font-size: 2em; border-radius: 13px; text-align: center; margin-left: 0; }
    .button { font-size: 2em; width: 260px; min-width: 10ch; max-width: 20ch; padding: 11px; border: 2.75px solid #ff00ff; color: #00ffff; background: transparent; font-family: 'Orbitron', sans-serif; border-radius: 13px; margin: 10px 0 26px 0; cursor: pointer; display: inline-block; white-space: nowrap; }
    .mode-row { font-size: 2em; margin: 26px auto 0 auto; display: flex; justify-content: center; align-items: center; gap: 16px; }
    .mode-btn-row { display: flex; justify-content: center; margin: 10px auto 26px auto; }
    #profileInput { width: 260px; min-width: 10ch; max-width: 20ch; font-size: 2em; }
  </style>
</head>
<body>
  <h2>RiiWynch Engine Settings</h2>
  <div id="statusMessage" class="status-message"></div>
  <div class="mode-row"><input type="text" id="profileInput" value=")rawliteral";
  html_part1 += modeNames[manualMode ? 3 : currentProfile];
  html_part1 += R"rawliteral(" readonly></div>
  <div class="mode-btn-row"><button type="button" class="button" onclick="switchProfile()">Change Mode</button></div>
  <form id="settingsForm" onsubmit="return false;">)rawliteral";
  server.sendContent(html_part1);
  
  // Second chunk - form fields with values
  String html_part2 = "";
  html_part2 += "<div class=\"form-row\"><label>Starter Relay Time (ms):</label><input name=\"starterRelayTime\" type=\"text\" value=\"" + String(starterRelayTime) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Ramp Up Duration (ms):</label><input name=\"rampUpDuration\" type=\"text\" value=\"" + String(rampUpDuration) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Ramp-Up Exponent:</label><input name=\"rampUpExponent\" type=\"text\" value=\"" + String(rampUpExponent, 2) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Ramp Down Duration (ms):</label><input name=\"rampDownDuration\" type=\"text\" value=\"" + String(rampDownDuration) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Gas Idle Angle (°):</label><input name=\"gasIdleAngle\" type=\"text\" value=\"" + String(gasIdleAngle) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Gas Max Angle (°):</label><input name=\"gasMaxAngle\" type=\"text\" value=\"" + String(gasMaxAngle) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Choke Angle (°):</label><input name=\"chokeAngle\" type=\"text\" value=\"" + String(chokeAngle) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Brake Angle (°):</label><input name=\"brakeAngle\" type=\"text\" value=\"" + String(brakeAngle) + "\"></div>";
  html_part2 += "<div class=\"form-row\"><label>Stop Cooldown (ms):</label><input name=\"stopCooldownDuration\" type=\"text\" value=\"" + String(stopCooldownDuration) + "\"></div>";
  
  // Send second chunk
  server.sendContent(html_part2);
  
  // Third chunk - buttons
  String html_part3 = R"rawliteral(
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
          document.querySelector('input[name="rampUpDuration"]').value = data.rampUpDuration;
          document.querySelector('input[name="rampUpExponent"]').value = data.rampUpExponent;
          document.querySelector('input[name="rampDownDuration"]').value = data.rampDownDuration;
          document.querySelector('input[name="gasIdleAngle"]').value = data.gasIdleAngle;
          document.querySelector('input[name="gasMaxAngle"]').value = data.gasMaxAngle;
          document.querySelector('input[name="chokeAngle"]').value = data.chokeAngle;
          document.querySelector('input[name="brakeAngle"]').value = data.brakeAngle;
          document.querySelector('input[name="stopCooldownDuration"]').value = data.stopCooldownDuration;
          showStatusMessage('Switched to Mode ' + modeNames[data.manualMode ? 3 : (data.profile-1)]);
        })
        .catch(error => { showStatusMessage('Error switching mode: ' + error, false); });
    }
    // On page load, sync mode box
    window.onload = updateModeBox;
  </script>
</body>
</html>)rawliteral";
  server.sendContent(html_part3);
  server.sendContent("");
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
  
  if (server.hasArg("rampUpDuration")) {
    rampUpDuration = server.arg("rampUpDuration").toInt();
  }
  
  if (server.hasArg("rampUpExponent")) {
    // Convert to float with bounds checking
    float newExp = server.arg("rampUpExponent").toFloat();
    if (newExp > 0) { // Prevent division by zero
      rampUpExponent = newExp;
    }
  }
  
  if (server.hasArg("rampDownDuration")) {
    rampDownDuration = server.arg("rampDownDuration").toInt();
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
  
  if (server.hasArg("rampUpDuration")) {
    rampUpDuration = server.arg("rampUpDuration").toInt();
  }
  
  if (server.hasArg("rampUpExponent")) {
    // Convert to float with bounds checking
    float newExp = server.arg("rampUpExponent").toFloat();
    if (newExp > 0) { // Prevent division by zero
      rampUpExponent = newExp;
    }
  }
  
  if (server.hasArg("rampDownDuration")) {
    rampDownDuration = server.arg("rampDownDuration").toInt();
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
}


  }
  server.send(200, "text/plain", "OK");
}

// Add a new handler to switch profiles
void handleSwitchProfile() {
  // Save current settings to the current profile
  if (manualMode) currentProfile = 3;
  saveSettingsForProfile(currentProfile);
  
  // The saveSettingsForProfile function should handle EEPROM commit internally
  
  Serial.printf("💾 Saved settings to profile %d before switching\n", currentProfile + 1);
  
  // Cycle to the next profile (0-3)
  currentProfile = (currentProfile + 1) % 4;
  manualMode = (currentProfile == 3);
  Serial.printf("🔄 Switching to profile %d\n", currentProfile + 1);
  
  // Load settings for the new profile
  loadSettingsForProfile(currentProfile);
  
  // Prepare JSON response with all loaded settings
  String jsonResponse = "{";
  jsonResponse += "\"profile\":" + String(currentProfile + 1) + ",";
  jsonResponse += "\"starterTime\":" + String(starterRelayTime) + ",";
  jsonResponse += "\"rampUpDuration\":" + String(rampUpDuration) + ",";
  jsonResponse += "\"rampUpExponent\":" + String(rampUpExponent, 2) + ",";
  jsonResponse += "\"rampDownDuration\":" + String(rampDownDuration) + ",";
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
