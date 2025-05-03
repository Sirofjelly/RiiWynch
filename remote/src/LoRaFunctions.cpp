#include "LoRaFunctions.h"
#include <Arduino.h>
#include "LoRaWan_APP.h"

#define RF_FREQUENCY                868000000
#define TX_OUTPUT_POWER             14
#define LORA_BANDWIDTH              0    // 125 kHz
#define LORA_SPREADING_FACTOR       8
#define LORA_CODINGRATE             1    // 4/5
#define LORA_PREAMBLE_LENGTH        8
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false

static RadioEvents_t RadioEvents;
char loraPacketBuffer[256];
bool loraNewDataReceived = false;
int loraRSSI = 0;
int loraSNR = 0;

void initLoRa() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.RxDone     = OnRxDone;
  RadioEvents.RxTimeout  = OnRxTimeout;
  RadioEvents.RxError    = OnRxError;
  RadioEvents.TxDone     = OnTxDone;
  RadioEvents.TxTimeout  = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR, LORA_CODINGRATE, LORA_PREAMBLE_LENGTH,
    LORA_FIX_LENGTH_PAYLOAD_ON, true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH, 0,
    LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
    LORA_IQ_INVERSION_ON, true);

  Radio.Rx(0);
}

void processLoRa() {
  Radio.IrqProcess();
}

void sendLoRaPacket(const char* payload) {
  Radio.Send((uint8_t*)payload, strlen(payload));
  Serial.print("LoRa TX: ");
  Serial.println(payload);
}

bool parseLoRaPacket(const char* raw, char* type, int* value, long* counter) {
  return (sscanf(raw, "%3[^,],%d,%ld", type, value, counter) == 3);
}

// ---------------------
// CALLBACKS
// ---------------------
void OnRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr) {
  Radio.Sleep();

  if (size >= sizeof(loraPacketBuffer)) size = sizeof(loraPacketBuffer) - 1;
  memcpy(loraPacketBuffer, payload, size);
  loraPacketBuffer[size] = '\0';

  loraRSSI = rssi;
  loraSNR = snr;
  loraNewDataReceived = true;

  Serial.print("LoRa RX: ");
  Serial.print(loraPacketBuffer);
  Serial.print("  RSSI: ");
  Serial.print(rssi);
  Serial.print("  SNR: ");
  Serial.println(snr);

  Radio.Rx(0);
}

void OnTxDone() {
  Serial.println("LoRa TX done.");
  Radio.Rx(0);
}

void OnTxTimeout() {
  Serial.println("LoRa TX timeout!");
  Radio.Rx(0);
}

void OnRxTimeout() {
  Serial.println("LoRa RX timeout.");
  Radio.Rx(0);
}

void OnRxError() {
  Serial.println("LoRa RX error.");
  Radio.Rx(0);
}
