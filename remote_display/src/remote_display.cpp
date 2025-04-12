/*******************************************************
 * COMBINED: LoRa Transmitter + OLED Display + Buttons with Sync
 *
 * Features:
 * 1. Sends:
 *    - "BTN,x,y"  (Button-press packet) [Assuming you have button state to send]
 *    - "VAL,x,y"  (Percentage packet)
 * 2. Receives:
 *    - "VAL,x,y"  (Percentage packet from receiver)
 * 3. Displays:
 *    - Local user input via UP/DOWN (targetPercentage)
 *    - Updates based on received "VAL" packets
 *******************************************************/

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include <U8g2lib.h>

// -----------------------------------
//  LORA DEFINITIONS & GLOBALS
// -----------------------------------
#define RF_FREQUENCY                868000000
#define TX_OUTPUT_POWER             14
#define LORA_BANDWIDTH              0    // 125 kHz
#define LORA_SPREADING_FACTOR       8
#define LORA_CODINGRATE             1    // 4/5
#define LORA_PREAMBLE_LENGTH        8
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false

// Forward declarations for LoRa callbacks
void OnRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxTimeout(void);
void OnRxError(void);

// LoRa variables
static RadioEvents_t RadioEvents;
char rxPacketBuffer[256];
int  lastButtonState   = 0;  // from "BTN,x"
int  lastPercentage    = 0;  // from "VAL,y"
long lastPacketCount   = 0;  // from the third field
bool newDataReceived   = false;

// Timeout logic for "BTN" packets
unsigned long lastButtonUpdateTime = 0;
const unsigned long buttonTimeoutMs = 1000; // 1 second

// Packet counter for outgoing packets
long packetCounter = 0;

// -----------------------------------
//  DISPLAY & BUTTON DEFINITIONS
// -----------------------------------

// Initialize the OLED for Heltec LoRa32 V3:
//   SDA = 19, SCL = 20 in your example
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  /* reset=*/ U8X8_PIN_NONE,
  /* clock=*/ 20,
  /* data=*/ 19
);

// Pins for local UP/DOWN buttons (adjust as needed)
const int upButtonPin = 7;
const int downButtonPin = 4;

// Debounce & press handling
int upButtonState          = HIGH;
int downButtonState        = HIGH;
int lastUpButtonState      = HIGH;
int lastDownButtonState    = HIGH;
unsigned long lastDebounceTimeUp     = 0;
unsigned long lastDebounceTimeDown   = 0;
unsigned long debounceDelay          = 20;
unsigned long pressTimeUp            = 0;
unsigned long pressTimeDown          = 0;

// For gradually updating the percentage displayed
unsigned long lastUpdateTime = 0;
unsigned long updateInterval = 10;  // ms between display increments
unsigned long lastChangeTime = 0;
unsigned long changeInterval = 200; // ms between repeated increments on hold

int targetPercentage    = 0;  // local user target
int displayedPercentage = 0;  // what's currently shown on screen

// -----------------------------------
//  FORWARD DECLARATIONS
// -----------------------------------
void handleButton(
  int buttonPin,
  int* buttonState,
  int* lastButtonState,
  unsigned long* pressTime,
  unsigned long* lastDebounceTime,
  void (*changePercentage)()
);

void increasePercentage();
void decreasePercentage();
void updateDisplay();
void drawThickerRoundedFrame();
void drawSlimRoundedBar(int percentage);
void sendValPacket(int percentage);

// -----------------------------------
//  SETUP
// -----------------------------------
void setup() {
  // ----------------
  // SERIAL FOR DEBUG
  // ----------------
  Serial.begin(115200);
  delay(100);

  // -------------
  // LORA SETUP
  // -------------
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.RxDone     = OnRxDone;
  RadioEvents.RxTimeout  = OnRxTimeout;
  RadioEvents.RxError    = OnRxError;
  RadioEvents.TxDone     = OnTxDone;
  RadioEvents.TxTimeout  = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetTxConfig(
    MODEM_LORA,
    TX_OUTPUT_POWER,
    0,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODINGRATE,
    LORA_PREAMBLE_LENGTH,
    LORA_FIX_LENGTH_PAYLOAD_ON,
    true,
    0,
    0,
    LORA_IQ_INVERSION_ON,
    3000
  );

  Radio.SetRxConfig(
    MODEM_LORA,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODINGRATE,
    0, // AFC Bandwidth (not used in LoRa)
    LORA_PREAMBLE_LENGTH,
    0,
    LORA_FIX_LENGTH_PAYLOAD_ON,
    0,
    true,
    0,
    0,
    LORA_IQ_INVERSION_ON,
    true
  );

  Radio.Rx(0); // start continuous RX
  lastButtonUpdateTime = millis();

  Serial.println("LoRa Transmitter initialized. Listening for packets...");

  // ----------------
  // OLED & BUTTONS
  // ----------------
  pinMode(upButtonPin, INPUT_PULLUP);
  pinMode(downButtonPin, INPUT_PULLUP);

  // Start the display
  u8g2.begin();
  // Show "STOP" initially
  targetPercentage = 0;
  displayedPercentage = 0;
  updateDisplay();

  // Initialize local button states
  upButtonState       = digitalRead(upButtonPin);
  downButtonState     = digitalRead(downButtonPin);
  lastUpButtonState   = upButtonState;
  lastDownButtonState = downButtonState;
}

// -----------------------------------
//  LOOP
// -----------------------------------
void loop()
{
  // ----------------
  // LoRa processing
  // ----------------
  Radio.IrqProcess();

  // If no BTN packet arrived recently, set lastButtonState=0
  if ((millis() - lastButtonUpdateTime) > buttonTimeoutMs) {
    if (lastButtonState != 0) {
      lastButtonState = 0;
      Serial.println("WARNING: No BTN data => lastButtonState=0 (timeout).");
      // Optionally, you can trigger actions based on button state here
    }
  }

  // If we got new data (BTN or VAL), show it
  if (newDataReceived) {
    newDataReceived = false;

    Serial.println(F("----- New LoRa Data Received -----"));
    Serial.print(F(" lastButtonState = "));
    Serial.println(lastButtonState);
    Serial.print(F(" lastPercentage  = "));
    Serial.println(lastPercentage);
    Serial.print(F(" lastPacketCount = "));
    Serial.println(lastPacketCount);
    Serial.println();

    // Update targetPercentage based on received VAL packet
    if (lastPercentage != targetPercentage) {
      targetPercentage = lastPercentage;
      // After updating, send VAL packet to inform receiver about this update
      sendValPacket(targetPercentage);
    }
    // Then the next call to updateDisplay() in the normal loop logic will gradually animate to it.
  }

  // -------------------------
  // Local Button Handling
  // -------------------------
  // The second parameter is the function pointer for what to do (increase or decrease)
  handleButton(upButtonPin, &upButtonState, &lastUpButtonState, &pressTimeUp, &lastDebounceTimeUp, increasePercentage);
  handleButton(downButtonPin, &downButtonState, &lastDownButtonState, &pressTimeDown, &lastDebounceTimeDown, decreasePercentage);

  // -------------------------
  // Gradually update display
  // -------------------------
  if (millis() - lastUpdateTime > updateInterval) {
    bool percentageChanged = false;

    if (displayedPercentage < targetPercentage) {
      displayedPercentage++;
      percentageChanged = true;
    } else if (displayedPercentage > targetPercentage) {
      displayedPercentage--;
      percentageChanged = true;
    }

    if (percentageChanged) {
      updateDisplay();
      // After updating display, if this was a result of local change, send VAL packet
      // To avoid sending multiple packets during gradual update, ensure we only send once per target change
      // Thus, we'll send in increasePercentage() and decreasePercentage() functions
    }

    lastUpdateTime = millis();
  }

  // Short delay (if desired)
  delay(5);
}

// -----------------------------------------
//  LORA CALLBACKS
// -----------------------------------------
void OnRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr) {
  Radio.Sleep();

  // Copy payload as string
  if (size >= sizeof(rxPacketBuffer)) {
    size = sizeof(rxPacketBuffer) - 1;
  }
  memcpy(rxPacketBuffer, payload, size);
  rxPacketBuffer[size] = ' ';

  Serial.print(F("Received packet: "));
  Serial.print(rxPacketBuffer);
  Serial.print(F(""  RSSI: "));
  Serial.print(rssi);
  Serial.print(F("  SNR: "));
  Serial.println(snr);

  // Parse
  char type[4];
  int value = 0;
  long counter = 0;

  int matches = sscanf(rxPacketBuffer, "%3[^,],%d,%ld", type, &value, &counter);
  if (matches == 3) {
    if (strcmp(type, "BTN") == 0) {
      lastButtonState = value;
      lastPacketCount = counter;
      lastButtonUpdateTime = millis();

      Serial.print(F("  -> Parsed as BTN, state="));
      Serial.print(lastButtonState);
      Serial.print(F(", count="));
      Serial.println(lastPacketCount);
    }
    else if (strcmp(type, "VAL") == 0) {
      lastPercentage  = value;
      lastPacketCount = counter;

      Serial.print(F("  -> Parsed as VAL, percentage="));
      Serial.print(lastPercentage);
      Serial.print(F(", count="));
      Serial.println(lastPacketCount);
    }
    else {
      Serial.println(F("  -> Unrecognized type!"));
    }
  }
  else {
    Serial.println(F("  -> Parsing error (expected "TYPE,val,count")."));
  }

  newDataReceived = true;
  Radio.Rx(0);
}

void OnTxDone(void) {
  Serial.println("OnTxDone: TX complete");
  Radio.Rx(0);
}

void OnRxTimeout(void) {
  Serial.println("OnRxTimeout");
  Radio.Rx(0);
}

void OnRxError(void) {
  Serial.println("OnRxError");
  Radio.Rx(0);
}

void OnTxTimeout(void) {
  Serial.println("OnTxTimeout");
  Radio.Rx(0);
}

// -----------------------------------------
//  LOCAL BUTTON HANDLING
// -----------------------------------------
void handleButton(
  int buttonPin,
  int* buttonState,
  int* lastButtonState,
  unsigned long* pressTime,
  unsigned long* lastDebounceTime,
  void (*changePercentage)()
) {
  int reading = digitalRead(buttonPin);

  if (reading != *lastButtonState) {
    *lastDebounceTime = millis();
  }

  // Debounce
  if ((millis() - *lastDebounceTime) > debounceDelay) {
    if (reading != *buttonState) {
      *buttonState = reading;

      // Pressed
      if (*buttonState == LOW) {
        *pressTime = millis();
        lastChangeTime = millis();
      }
      // Released
      else {
        if ((millis() - *pressTime) < 500) {
          // Short press
          changePercentage();
          // After changing percentage, send VAL packet
          sendValPacket(targetPercentage);
        }
        lastChangeTime = 0;
      }
    }
  }

  // Continuous change if held >500ms
  if (*buttonState == LOW && (millis() - *pressTime) >= 500) {
    if ((millis() - lastChangeTime) > changeInterval) {
      changePercentage();
      // After changing percentage, send VAL packet
      sendValPacket(targetPercentage);
      lastChangeTime = millis();
    }
  }

  *lastButtonState = reading;
}

// Increase local targetPercentage
void increasePercentage() {
  if (targetPercentage < 100) {
    targetPercentage += 5;
    if (targetPercentage > 100) targetPercentage = 100;
  }
}

// Decrease local targetPercentage
void decreasePercentage() {
  if (targetPercentage > 0) {
    targetPercentage -= 5;
    if (targetPercentage < 0) targetPercentage = 0;
  }
}

// Send VAL packet to receiver
void sendValPacket(int percentage) {
  // Prepare the VAL packet string: "VAL,x,y"
  // where x = percentage, y = packetCounter
  char packet[20];
  snprintf(packet, sizeof(packet), "VAL,%d,%ld", percentage, packetCounter++);

  // Send the packet
  Radio.Send((uint8_t*)packet, strlen(packet));

  Serial.print("Sent VAL packet: ");
  Serial.println(packet);
}

// -----------------------------------------
//  OLED DISPLAY FUNCTIONS
// -----------------------------------------
void updateDisplay() {
  u8g2.clearBuffer();

  // Thicker frame
  drawThickerRoundedFrame();

  // If displayedPercentage == 0, show "STOP"
  u8g2.setFont(u8g2_font_logisoso42_tf);
  char text[6];
  if (displayedPercentage == 0) {
    snprintf(text, sizeof(text), "STOP");
  } else {
    snprintf(text, sizeof(text), "%d%%", displayedPercentage);
  }

  int16_t width = u8g2.getStrWidth(text);
  int x = (128 - width) / 2;
  int y = 47;
  u8g2.drawStr(x, y, text);

  // Draw the slim progress bar
  drawSlimRoundedBar(displayedPercentage);

  u8g2.sendBuffer();

  Serial.print(F("OLED updated to: "));
  if (displayedPercentage == 0) {
    Serial.println("STOP");
  } else {
    Serial.print(displayedPercentage);
    Serial.println("%");
  }
}

void drawThickerRoundedFrame() {
  int thickness = 3;
  int cornerRadius = 8;

  // Outer filled rectangle with rounded corners
  u8g2.drawRBox(0, 0, 128, 64, cornerRadius);

  // Inner region to create thickness
  u8g2.setDrawColor(0);
  u8g2.drawRBox(
    thickness, thickness,
    128 - 2 * thickness,
    64 - 2 * thickness,
    cornerRadius - thickness
  );
  u8g2.setDrawColor(1);
}

void drawSlimRoundedBar(int percentage) {
  int barX         = 6;
  int barY         = 51;
  int barHeight    = 6;
  int maxBarWidth  = 116;

  int barWidth = (percentage * maxBarWidth) / 100;
  barWidth = constrain(barWidth, 0, maxBarWidth);

  int cornerRadius = (barWidth < 6) ? barWidth / 2 : 3;

  if (barWidth > 0) {
    u8g2.drawRBox(barX, barY, barWidth, barHeight, cornerRadius);
  }

  // Outline
  u8g2.drawRFrame(barX - 1, barY - 1, maxBarWidth + 2, barHeight + 2, 3);
} 