#include <Arduino.h>
#include "NetworkManager.h"
#include "SdManager.h"
#include "SpiManager.h"
#include "CommandProcessor.h"

NetworkManager   net;
SdManager        sdMgr;
SpiManager       spi;
CommandProcessor cmdProc(net, sdMgr, spi);

elapsedMillis sinceIpPrint;
static constexpr uint32_t IP_PRINT_INTERVAL_MS = 5000;

void blinkStartupPattern();
void setupInterruptPriorities();

void setup() {
  Serial.begin(115200);

  sdMgr.begin();
  net.begin();
  cmdProc.begin();

  blinkStartupPattern();

  spi.begin();
  setupInterruptPriorities();
}

void loop() {
  net.serviceTcp();         // 1. Accept client, read and parse commands
  cmdProc.processCommand(); // 2. Handle one parsed command
  cmdProc.serviceDisplay(); // 3. Pattern playback, frame transfers
  net.flushResponses();     // 4. Send ready responses over TCP

  if (Serial && sinceIpPrint >= IP_PRINT_INTERVAL_MS) {
    sinceIpPrint = 0;
    Serial.printf("IP: %s\n", net.ipAddress());
  }
}

void blinkStartupPattern() {
  static constexpr uint8_t LED_PIN = LED_BUILTIN;
  static constexpr uint32_t SHORT_MS = 100;
  static constexpr uint32_t LONG_MS = 300;
  static constexpr uint32_t PAUSE_MS = 100;

  pinMode(LED_PIN, OUTPUT);

  // Pattern: 3 long, 1 long, 1 short, 1 long
  const uint32_t pattern[] = {LONG_MS, LONG_MS,  LONG_MS,
                              LONG_MS, SHORT_MS, LONG_MS};

  for (uint32_t dur : pattern) {
    digitalWriteFast(LED_PIN, HIGH);
    delay(dur);
    digitalWriteFast(LED_PIN, LOW);
    delay(PAUSE_MS);
  }
}

void setupInterruptPriorities() {
  // SPI highest, then Ethernet, then SDIO
  NVIC_SET_PRIORITY(IRQ_LPSPI4, 0);   // SPI  — highest
  NVIC_SET_PRIORITY(IRQ_LPSPI3, 0);   // SPI1 — highest
  NVIC_SET_PRIORITY(IRQ_ENET,   64);  // Ethernet — mid
  NVIC_SET_PRIORITY(IRQ_SDHC1,  96);  // SDIO — lowest
}
