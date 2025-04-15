#undef LORA_DEFAULT_RESET_PIN
#undef LORA_DEFAULT_DIO0_PIN
#include "ESP32_LoRaWAN.h"
#include <LoRa.h>

#define BAND 915E6 // LoRa frequency band

// Variables to store speed percentage
int speedPercentage = 0;

void setupLoRaWAN() {
    // Initialize LoRaWAN with default parameters
    LoRaWAN.init(CLASS_A, LORAMAC_REGION_EU868);
    Serial.println("LoRaWAN initialized.");
}

void setup() {
    Serial.begin(115200);
    setupLoRaWAN();

    // Join the network
    LoRaWAN.join();
    Serial.println("LoRaWAN joined.");
}

void loop() {
    // Send data periodically
    LoRaWAN.send(CLASS_A);
    delay(1000); // Adjust delay as needed
}