#include <WiFi.h>
#include <WebServer.h>
#include "Settings.h"

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

void handleRoot() {
  String html = R"rawliteral(
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
      font-size: 2.25em; /* 1.5em → 2.25em */
      border-radius: 10px;
      text-align: center;
    }
    .form-row {
      margin: 18px auto;
    }
    .button {
      font-size: 2.7em; /* 1.8em → 2.7em */
      padding: 14px 28px;
      border: 3px solid #ff00ff;
      color: #00ffff;
      background: transparent;
      font-family: 'Orbitron', sans-serif;
      border-radius: 10px;
      margin: 30px 10px;
    }
    .switch-container {
      display: flex;
      justify-content: center;
      align-items: center;
      margin-top: 35px;
      font-size: 2.25em; /* 1.5em → 2.25em */
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
  <form method="POST" action="/set">
    <div class="form-row"><label>Starter Relay Time (ms):</label><input name="starterRelayTime" type="text" value="%START%"></div>
    <div class="form-row"><label>Ramp Up Duration (ms):</label><input name="rampUpDuration" type="text" value="%RUP%"></div>
    <div class="form-row"><label>Ramp-Up Exponent:</label><input name="rampUpExponent" type="text" value="%EXPO%"></div>
    <div class="form-row"><label>Ramp Down Duration (ms):</label><input name="rampDownDuration" type="text" value="%RDOWN%"></div>
    <div class="form-row"><label>Gas Idle Angle (°):</label><input name="gasIdleAngle" type="text" value="%IDLE%"></div>
    <div class="form-row"><label>Gas Max Angle (°):</label><input name="gasMaxAngle" type="text" value="%MAX%"></div>
    <div class="form-row"><label>Choke Angle (°):</label><input name="chokeAngle" type="text" value="%CHOKE%"></div>
    <div class="form-row"><label>Brake Angle (°):</label><input name="brakeAngle" type="text" value="%BRAKE%"></div>
    <div class="form-row"><label>Stop Cooldown (ms):</label><input name="stopCooldownDuration" type="text" value="%STOPCD%"></div>
    <input type="submit" value="Apply" class="button">
  </form>

  <div class="switch-container">
    <span class="switch-label">Manual Mode:</span>
    <label class="switch">
      <input type="checkbox" onchange="toggleManualMode(this)" %MANUALMODE%>
      <span class="slider"></span>
    </label>
  </div>

  <script>
    function toggleManualMode(checkbox) {
      var xhttp = new XMLHttpRequest();
      xhttp.open("GET", "/toggleManual?state=" + (checkbox.checked ? "1" : "0"), true);
      xhttp.send();
    }
  </script>
</body>
</html>
)rawliteral";

  html.replace("%START%", String(starterRelayTime));
  html.replace("%RUP%", String(rampUpDuration));
  html.replace("%EXPO%", String(rampUpExponent, 2));
  html.replace("%RDOWN%", String(rampDownDuration));
  html.replace("%IDLE%", String(gasIdleAngle));
  html.replace("%MAX%", String(gasMaxAngle));
  html.replace("%CHOKE%", String(chokeAngle));
  html.replace("%BRAKE%", String(brakeAngle));
  html.replace("%STOPCD%", String(stopCooldownDuration));
  html.replace("%MANUALMODE%", manualMode ? "checked" : "");

  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("starterRelayTime")) starterRelayTime = server.arg("starterRelayTime").toInt();
  if (server.hasArg("rampUpDuration")) rampUpDuration = server.arg("rampUpDuration").toInt();
  if (server.hasArg("rampUpExponent")) rampUpExponent = server.arg("rampUpExponent").toFloat();
  if (server.hasArg("rampDownDuration")) rampDownDuration = server.arg("rampDownDuration").toInt();
  if (server.hasArg("gasIdleAngle")) gasIdleAngle = server.arg("gasIdleAngle").toInt();
  if (server.hasArg("gasMaxAngle")) gasMaxAngle = server.arg("gasMaxAngle").toInt();
  if (server.hasArg("chokeAngle")) chokeAngle = server.arg("chokeAngle").toInt();
  if (server.hasArg("brakeAngle")) brakeAngle = server.arg("brakeAngle").toInt();
  if (server.hasArg("stopCooldownDuration")) stopCooldownDuration = server.arg("stopCooldownDuration").toInt();

  saveSettings();
  handleRoot();
}

void handleToggleManual() {
  if (server.hasArg("state")) {
    manualMode = server.arg("state") == "1";
  }
  server.send(200, "text/plain", "OK");
}

void setupWebUI() {
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/toggleManual", handleToggleManual);
  server.begin();
}

void handleWebUI() {
  server.handleClient();
}
