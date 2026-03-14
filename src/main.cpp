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

void setupInterruptPriorities();

void setup() {
  Serial.begin(115200);

  sdMgr.begin();
  spi.begin();
  net.begin();
  cmdProc.begin();

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

void setupInterruptPriorities() {
  // SPI highest, then Ethernet, then SDIO
  NVIC_SET_PRIORITY(IRQ_LPSPI4, 0);   // SPI  — highest
  NVIC_SET_PRIORITY(IRQ_LPSPI3, 0);   // SPI1 — highest
  NVIC_SET_PRIORITY(IRQ_ENET,   64);  // Ethernet — mid
  NVIC_SET_PRIORITY(IRQ_SDHC1,  96);  // SDIO — lowest
}
