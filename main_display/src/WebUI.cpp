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
  loadSettingsForProfile(currentProfile); // Always load current profile's values before rendering
  
  // Begin sending the HTML in chunks to avoid buffer issues
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); // Tell the client we don't know the exact content length
  server.send(200, "text/html", ""); // Send the header only
  
  // Send HTML in chunks
  // First chunk - the beginning of the HTML file
  String html_part1 = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>RiiWynch Settings</title>
  <link href="https://fonts.googleapis.com/css2?family=Orbitron&display=swap" rel="stylesheet">
  <style>
    body {
      background-color: #111;
      color: #00ffff;
      font-family: 'Orbitron', sans-serif;
      text-align: center;
      padding: 30px;
    }
    h2 {
      font-size: 3em;
      margin-bottom: 30px;
    }
    .status-message {
      background-color: rgba(0, 255, 0, 0.2);
      color: #00ff00;
      padding: 15px;
      margin: 20px auto;
      border-radius: 10px;
      border: 2px solid #00ff00;
      max-width: 80%;
      font-size: 2em;
      display: none;
    }
    label {
      display: inline-block;
      width: 600px;
      text-align: right;
      margin-right: 20px;
      font-size: 2.25em;
      vertical-align: middle;
    }
    input[type="text"] {
      width: 120px;
      padding: 10px;
      background: transparent;
      border: 3px solid #ff00ff;
      color: #00ffff;
      font-family: 'Orbitron', sans-serif;
      font-size: 2.25em;
      border-radius: 10px;
      text-align: center;
    }
    .form-row {
      margin: 18px auto;
    }
    .button {
      font-size: 2.7em;
      padding: 14px 28px;
      border: 3px solid #ff00ff;
      color: #00ffff;
      background: transparent;
      font-family: 'Orbitron', sans-serif;
      border-radius: 10px;
      margin: 30px 10px;
      cursor: pointer;
    }
    .switch-container {
      display: flex;
      justify-content: center;
      align-items: center;
      margin-top: 35px;
      font-size: 2.25em;
    }
    .switch-label {
      margin-right: 15px;
    }
    .switch {
      position: relative;
      display: inline-block;
      width: 70px;
      height: 40px;
    }
    .switch input {
      display: none;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      background-color: #ccc;
      border-radius: 40px;
      top: 0; left: 0; right: 0; bottom: 0;
      transition: .4s;
    }
    .slider:before {
      content: "";
      position: absolute;
      height: 30px;
      width: 30px;
      left: 5px;
      bottom: 5px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: #2196F3;
    }
    input:checked + .slider:before {
      transform: translateX(30px);
    }
  </style>
</head>
<body>
  <h2>RiiWynch Engine Settings</h2>
  <div id="statusMessage" class="status-message"></div>
  <div class="form-row"><label>Current Profile:</label><input type="text" id="profileInput" value="Auto )rawliteral";
  
  // Add the current profile number
  html_part1 += String(currentProfile + 1);
  html_part1 += R"rawliteral(" readonly></div>
  <button type="button" class="button" onclick="switchProfile()">Switch Profile</button>
  
  <form id="settingsForm" onsubmit="return false;">)rawliteral";
  
  // Send first chunk
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
  
  // Third chunk - buttons and manual mode switch
  String html_part3 = R"rawliteral(
    <button type="button" class="button" onclick="applySettings()">Apply</button>
    <button type="button" class="button" onclick="saveSettings()">Save</button>
  </form>

  <div class="switch-container">
    <span class="switch-label">Manual Mode:</span>
    <label class="switch">
      <input type="checkbox" onchange="toggleManualMode(this)" )rawliteral";
  
  // Add manual mode status
  html_part3 += manualMode ? "checked" : "";
  
  html_part3 += R"rawliteral(>
      <span class="slider"></span>
    </label>
  </div>

  <script>
    function showStatusMessage(message, isSuccess = true) {
      const statusElem = document.getElementById('statusMessage');
      statusElem.style.display = 'block';
      statusElem.style.backgroundColor = isSuccess ? 'rgba(0, 255, 0, 0.2)' : 'rgba(255, 0, 0, 0.2)';
      statusElem.style.borderColor = isSuccess ? '#00ff00' : '#ff0000';
      statusElem.style.color = isSuccess ? '#00ff00' : '#ff0000';
      statusElem.innerHTML = message;
      
      // Auto-hide after 5 seconds
      setTimeout(() => {
        statusElem.style.display = 'none';
      }, 5000);
    }

    function applySettings() {
      const formData = new FormData(document.getElementById('settingsForm'));
      
      fetch('/set', {
        method: 'POST',
        body: formData
      })
      .then(response => response.text())
      .then(data => {
        showStatusMessage('Settings applied successfully!');
      })
      .catch(error => {
        showStatusMessage('Error applying settings: ' + error, false);
      });
    }

    function saveSettings() {
      const form = document.getElementById('settingsForm');
      const formData = new FormData(form);
      fetch('/save', {
        method: 'POST',
        body: formData
      })
      .then(response => response.json())
      .then(data => {
        if(data.success) {
          showStatusMessage('Settings saved successfully');
        } else {
          showStatusMessage('Error saving settings', false);
        }
      })
      .catch(error => {
        showStatusMessage('Error saving settings: ' + error, false);
      });
      return false; // Prevent form submission
    }
    
    function toggleManualMode(checkbox) {
      fetch('/toggleManual?state=' + (checkbox.checked ? '1' : '0'))
        .then(response => response.text())
        .then(data => {
          showStatusMessage('Manual mode ' + (checkbox.checked ? 'enabled' : 'disabled'));
        })
        .catch(error => {
          showStatusMessage('Error toggling manual mode: ' + error, false);
        });
    }
    
    function switchProfile() {
      fetch('/switchProfile')
        .then(response => response.json())
        .then(data => {
          // Update the profile display without reloading
          document.getElementById('profileInput').value = 'Auto ' + data.profile;
          
          // Update all form fields with the values from the new profile
          document.querySelector('input[name="starterRelayTime"]').value = data.starterTime;
          document.querySelector('input[name="rampUpDuration"]').value = data.rampUpDuration;
          document.querySelector('input[name="rampUpExponent"]').value = data.rampUpExponent;
          document.querySelector('input[name="rampDownDuration"]').value = data.rampDownDuration;
          document.querySelector('input[name="gasIdleAngle"]').value = data.gasIdleAngle;
          document.querySelector('input[name="gasMaxAngle"]').value = data.gasMaxAngle;
          document.querySelector('input[name="chokeAngle"]').value = data.chokeAngle;
          document.querySelector('input[name="brakeAngle"]').value = data.brakeAngle;
          document.querySelector('input[name="stopCooldownDuration"]').value = data.stopCooldownDuration;
          
          // Update manual mode checkbox
          document.querySelector('.switch input[type="checkbox"]').checked = data.manualMode === true;
          
          // Show a success message
          showStatusMessage('Switched to Profile ' + data.profile);
        })
        .catch(error => {
          showStatusMessage('Error switching profile: ' + error, false);
        });
    }
  </script>
</body>
</html>)rawliteral";
  
  // Send final chunk
  server.sendContent(html_part3);
  
  // End response
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
  saveSettingsForProfile(currentProfile);
  
  // The saveSettingsForProfile function should handle EEPROM commit internally
  
  Serial.printf("💾 Saved settings to profile %d before switching\n", currentProfile + 1);
  
  // Cycle to the next profile
  currentProfile = (currentProfile + 1) % totalProfiles;
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

void setupWebUI() {
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/save", HTTP_POST, handleSetDefault); // Change from "/set-default" to "/save"
  server.on("/toggleManual", handleToggleManual);
  server.on("/switchProfile", handleSwitchProfile);
  server.begin();
}

void handleWebUI() {
  server.handleClient();
}
