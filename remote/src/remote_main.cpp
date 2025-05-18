#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <RadioLib.h>

// ─── Display ───
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, 20, 19);

// ─── LoRa (SX1262) ───
#define L_CS   8
#define L_DIO1 14
#define L_RST  12
#define L_BUSY 13
Module  mod(L_CS, L_DIO1, L_RST, L_BUSY, SPI);
SX1262  radio(&mod);

// ─── Pins ───
#define UP_BTN    7
#define DOWN_BTN  4
#define VBAT      1
#define ADC_CTRL 37
#define VEXT     21

// ─── States ───
enum State { START, MENU };
State state = START;

// ─── Button States ───
int upState = HIGH, lastUp = HIGH;
int downState = HIGH, lastDown = HIGH;
unsigned long debUp = 0, debDown = 0;
const unsigned long DEBOUNCE_MS = 20;

// ─── Menu ───
int targetPct = 0, shownPct = 0;
unsigned long pressUp = 0, pressDown = 0;
unsigned long lastChangeTime = 0;
unsigned long lastUpdateTime = 0;
unsigned long lastActivity = 0;
const unsigned long REPEAT_MS = 200;
const unsigned long MENU_TIMEOUT_MS = 1500;

// ─── START screen update ───
unsigned long lastStartUpdate = 0;
const unsigned long START_UPDATE_MS = 500;

// ─── Triple-press (sliding window) ───
unsigned long tapTimes[3] = {0, 0, 0};

// ─── Transmission Timers ───
unsigned long lastBtnTx = 0;
unsigned long lastHB = 0;
const unsigned long BTN_TX_MS = 50;
const unsigned long HB_MS = 500;
char txBuf[32];
uint16_t pktCnt = 0;

// ─── Prototypes ───
void drawStart(), drawMenu();
void sendBtn(uint8_t), sendPct(int);
void incPct(), decPct();
uint16_t readBattery();
void handleButton(int, int&, int&, unsigned long&, unsigned long&, void(*)());
void registerTripleTap();

// ─────────────────────────────────────────
//                 SETUP
// ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Setup of Remote...");

  pinMode(UP_BTN, INPUT_PULLUP);
  pinMode(DOWN_BTN, INPUT_PULLUP);
  pinMode(ADC_CTRL, OUTPUT); digitalWrite(ADC_CTRL, HIGH);
  pinMode(VEXT, OUTPUT);     digitalWrite(VEXT, LOW);
  analogReadResolution(12);

  u8g2.begin();
  drawStart();

  SPI.begin();
  int err = radio.begin(868.0);
  if (err != RADIOLIB_ERR_NONE) {
    Serial.print("LoRa init failed: ");
    Serial.println(err);
    // Don't hang — continue without radio
  } else {
    radio.setOutputPower(14);
    radio.setSpreadingFactor(8);
    radio.setCodingRate(5);
    radio.setBandwidth(125.0);
    Serial.println("LoRa ready.");
  }
}

// ─────────────────────────────────────────
//                 LOOP
// ─────────────────────────────────────────
void loop() {

  switch (state) {

    // ─── START SCREEN ───
    case START: {
      int r = digitalRead(UP_BTN);
      if (r != lastUp) debUp = millis();
      if (millis() - debUp > DEBOUNCE_MS && r != upState) {
        upState = r;
        if (upState == LOW) {
          registerTripleTap();
        }
      }
      lastUp = r;

      if (millis() - lastStartUpdate >= START_UPDATE_MS) {
        drawStart();
        lastStartUpdate = millis();
      }

      if (millis() - lastBtnTx >= BTN_TX_MS) {
        lastBtnTx = millis();
        bool anyPressed = (digitalRead(UP_BTN) == LOW || digitalRead(DOWN_BTN) == LOW);
        sendBtn(anyPressed ? 1 : 0);
      }
    } break;

    // ─── MENU SCREEN ───
    case MENU: {
      if (digitalRead(UP_BTN) == LOW || digitalRead(DOWN_BTN) == LOW) {
        lastActivity = millis();
      }
      if (millis() - lastActivity > MENU_TIMEOUT_MS) {
        sendPct(targetPct);
        state = START;
        drawStart();
        lastStartUpdate = millis();
        break;
      }

      if (millis() - lastHB >= HB_MS) {
        lastHB = millis();
        sendBtn(0);
      }

      handleButton(UP_BTN, upState, lastUp, pressUp, debUp, incPct);
      handleButton(DOWN_BTN, downState, lastDown, pressDown, debDown, decPct);

      if (millis() - lastUpdateTime > 10 && shownPct != targetPct) {
        shownPct += (shownPct < targetPct) ? 1 : -1;
        drawMenu();
        lastUpdateTime = millis();
      }
    } break;
  }
}

// ─────────────────────────────────────────
//             TRIPLE PRESS LOGIC
// ─────────────────────────────────────────
void registerTripleTap() {
  tapTimes[0] = tapTimes[1];
  tapTimes[1] = tapTimes[2];
  tapTimes[2] = millis();

  if (tapTimes[0] > 0 && (tapTimes[2] - tapTimes[0] <= 1500)) {
    Serial.println("TRIPLE PRESS  →  MENU");
    tapTimes[0] = tapTimes[1] = tapTimes[2] = 0;
    sendBtn(0);
    state = MENU;
    lastActivity = millis();
    lastHB = millis();
    lastStartUpdate = millis();
    drawMenu();
  }
}

// ─────────────────────────────────────────
//         BUTTON HOLD / REPEAT HANDLER
// ─────────────────────────────────────────
void handleButton(int pin, int &state, int &last, unsigned long &pressT, unsigned long &deb, void (*change)()) {
  int r = digitalRead(pin);
  if (r != last) deb = millis();
  if (millis() - deb > DEBOUNCE_MS && r != state) {
    state = r;
    if (state == LOW) {
      pressT = millis();
      lastChangeTime = millis();
    } else {
      if (millis() - pressT < 500) {
        change();
      }
      lastChangeTime = 0;
    }
  }
  if (state == LOW && millis() - pressT >= 500 && millis() - lastChangeTime > REPEAT_MS) {
    change();
    lastChangeTime = millis();
  }
  last = r;
}

// ─────────────────────────────────────────
//                ACTIONS
// ─────────────────────────────────────────
void incPct() {
  if (targetPct < 100) {
    targetPct += 5;
    drawMenu();
    Serial.printf("INC → %d\n", targetPct);
  }
}
void decPct() {
  if (targetPct > 0) {
    targetPct -= 5;
    drawMenu();
    Serial.printf("DEC → %d\n", targetPct);
  }
}
void sendBtn(uint8_t v) {
  sprintf(txBuf, "BTN,%d,%u", v, pktCnt++);
  radio.transmit((uint8_t *)txBuf, strlen(txBuf));
}
void sendPct(int p) {
  sprintf(txBuf, "VAL,%d,%u", p, pktCnt++);
  radio.transmit((uint8_t *)txBuf, strlen(txBuf));
}

// ─────────────────────────────────────────
//                DRAWING
// ─────────────────────────────────────────
void drawFrame() {
  const int t = 3, r = 8;
  u8g2.drawRBox(0, 0, 128, 64, r);
  u8g2.setDrawColor(0);
  u8g2.drawRBox(t, t, 128 - 2 * t, 64 - 2 * t, r - t);
  u8g2.setDrawColor(1);
}
void drawBar(int pct) {
  int x = 6, y = 51, h = 6, maxW = 116;
  int w = constrain(pct, 0, 100) * maxW / 100;
  int rad = (w < 6) ? w / 2 : 3;
  if (w > 0) u8g2.drawRBox(x, y, w, h, rad);
  u8g2.drawRFrame(x - 1, y - 1, maxW + 2, h + 2, 3);
}
void drawStart() {
  u8g2.clearBuffer(); drawFrame();
  u8g2.setFont(u8g2_font_logisoso28_tf);
  int w = u8g2.getStrWidth("START");
  u8g2.drawStr((128 - w) / 2, 46, "START");

  // Battery voltage
  char buf[8];
  sprintf(buf, "%.2fV", readBattery() / 1000.0);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(128 - u8g2.getStrWidth(buf) - 6, 13, buf);  // <-- shifted left/down

  u8g2.sendBuffer();
}
void drawMenu() {
  u8g2.clearBuffer(); drawFrame();
  char txt[6];
  if (shownPct == 0) strcpy(txt, "STOP");
  else sprintf(txt, "%d%%", shownPct);
  u8g2.setFont(u8g2_font_logisoso42_tf);
  int w = u8g2.getStrWidth(txt);
  u8g2.drawStr((128 - w) / 2, 47, txt);
  drawBar(shownPct);
  u8g2.sendBuffer();
}

// ─────────────────────────────────────────
//              BATTERY READ
// ─────────────────────────────────────────
uint16_t readBattery() {
  const float VREF = 3.3;
  const int MAX = 4095;
  const float DIV = 5.15;
  digitalWrite(ADC_CTRL, LOW); delay(20);
  int raw = analogRead(VBAT);
  digitalWrite(ADC_CTRL, HIGH);
  return (uint16_t)((raw * VREF / MAX) * DIV * 1000);
}