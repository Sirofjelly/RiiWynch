#include <WiFi.h>
#include <WebServer.h>
#include "Servos.h"
#include "Relays.h"
#include "StartupManager.h"
#include "Settings.h"

const char* ssid = "RiiWynch";
const char* password = "912345678";

WebServer server(80);

extern unsigned long starterRelayTime;
extern unsigned long rampUpDuration;
extern unsigned long rampDownDuration;
extern int chokeAngle;
extern int brakeAngle;
extern unsigned long stopCooldownDuration;
extern int gasIdleAngle;
extern int gasMaxAngle;

void handleRoot() {
  String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <title>Settings</title>
  <link href="https://fonts.googleapis.com/css2?family=Orbitron&display=swap" rel="stylesheet">
  <style>
    body {
      font-family: 'Orbitron', sans-serif;
      background: #111;
      color: #0f0;
      padding: 20px;
      font-size: 2em;
    }
    input {
      width: 150px;
      margin: 10px;
      font-family: 'Orbitron', sans-serif;
      font-size: 1em;
      background: transparent;
      border: 2px solid #00f;
      color: #0f0;
      padding: 5px 10px;
    }
    label {
      display: inline-block;
      width: 300px;
    }
    h2 {
      font-size: 2em;
      color: #0f0;
    }
  </style>
</head>
<body>
  <h2>RiiWynch Engine Settings</h2>
  <form method="POST" action="/set">
    <label>Starter Relay Time (ms):</label><input name="starterRelayTime" value="%START%"><br>
    <label>Ramp Up Duration (ms):</label><input name="rampUpDuration" value="%RUP%"><br>
    <label>Ramp Down Duration (ms):</label><input name="rampDownDuration" value="%RDOWN%"><br>
    <label>Gas Idle Angle (°):</label><input name="gasIdleAngle" value="%IDLE%"><br>
    <label>Gas Max Angle (°):</label><input name="gasMaxAngle" value="%MAX%"><br>
    <label>Choke Angle (°):</label><input name="chokeAngle" value="%CHOKE%"><br>
    <label>Brake Angle (°):</label><input name="brakeAngle" value="%BRAKE%"><br>
    <label>Stop Cooldown (ms):</label><input name="stopCooldownDuration" value="%STOPCD%"><br>
    <br><input type="submit" value="Apply">
  </form>
</body>
</html>
<!DOCTYPE html>
<html>
<head>
  <title>Settings</title>
  <link href="https://fonts.googleapis.com/css2?family=Orbitron&display=swap" rel="stylesheet">
  <style>
    body { font-family: 'Orbitron', sans-serif; background: #111; color: #0f0; padding: 20px; }
    input { width: 100px; margin: 5px; }
    label { display: inline-block; width: 200px; }
  </style>
</head>
<body>
  <h2>RiiWynch Engine Settings</h2>
  <form method="POST" action="/set">
    <label>Starter Relay Time (ms):</label><input name="starterRelayTime" value="%START%"><br>
    <label>Ramp Up Duration (ms):</label><input name="rampUpDuration" value="%RUP%"><br>
    <label>Ramp Down Duration (ms):</label><input name="rampDownDuration" value="%RDOWN%"><br>
    <label>Gas Idle Angle (°):</label><input name="gasIdleAngle" value="%IDLE%"><br>
    <label>Gas Max Angle (°):</label><input name="gasMaxAngle" value="%MAX%"><br>
    <label>Choke Angle (°):</label><input name="chokeAngle" value="%CHOKE%"><br>
    <label>Brake Angle (°):</label><input name="brakeAngle" value="%BRAKE%"><br>
    <label>Stop Cooldown (ms):</label><input name="stopCooldownDuration" value="%STOPCD%"><br>
    <br><input type="submit" value="Apply">
  </form>
</body>
</html>
)rawliteral";