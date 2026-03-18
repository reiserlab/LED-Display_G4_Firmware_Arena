#include <Arduino.h>
#include "NetworkManager.h"
#include "SdManager.h"
#include "SpiManager.h"
#include "CommandProcessor.h"

NetworkManager   net;
SdManager        sdMgr;
SpiManager       spi;
CommandProcessor cmdProc(net, sdMgr, spi);

#ifdef DEBUG_SERIAL
static bool ipPrinted = false;
#endif

void blinkStartupPattern();
void setupInterruptPriorities();

void setup() {
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
#endif

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

#ifdef DEBUG_SERIAL
  if (!ipPrinted && Serial && net.ipAddress()[0] != '\0') {
    DBG_PRINTF("MAC: %s  IP: %s\n", net.macAddress(), net.ipAddress());
    ipPrinted = true;
  }
#endif
}

void blinkStartupPattern() {
  static constexpr uint8_t LED_PIN = LED_BUILTIN;
  static constexpr uint32_t SHORT_MS = 100;
  static constexpr uint32_t LONG_MS = 300;
  static constexpr uint32_t PAUSE_MS = 100;

  pinMode(LED_PIN, OUTPUT);

  // Pattern: 3 long, [300 ms gap], 1 long, 1 short, 1 long
  static constexpr uint32_t GROUP_PAUSE_MS = 300;
  const uint32_t pattern[][3] = {
      {LONG_MS, LONG_MS,  LONG_MS}, // morse: O
      {LONG_MS, SHORT_MS, LONG_MS}  // morse: K
  };

  for (size_t g = 0; g < sizeof(pattern) / sizeof(pattern[0]); ++g) {
    if (g > 0) delay(GROUP_PAUSE_MS);
    for (size_t i = 0; i < sizeof(pattern[0]) / sizeof(pattern[0][0]); ++i) {
      digitalWriteFast(LED_PIN, HIGH);
      delay(pattern[g][i]);
      digitalWriteFast(LED_PIN, LOW);
      delay(PAUSE_MS);
    }
  }
}

void setupInterruptPriorities() {
  // SPI highest, then Ethernet, then SDIO
  NVIC_SET_PRIORITY(IRQ_LPSPI4, 0);   // SPI  — highest
  NVIC_SET_PRIORITY(IRQ_LPSPI3, 0);   // SPI1 — highest
  NVIC_SET_PRIORITY(IRQ_ENET,   64);  // Ethernet — mid
  NVIC_SET_PRIORITY(IRQ_SDHC1,  96);  // SDIO — lowest
}
