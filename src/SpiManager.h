#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <EventResponder.h>
#include "constants.h"

// Decoded frame structure matching the ArenaController panel layout.
struct QuarterPanel {
  uint8_t stretch;
  uint8_t data[AC::constants::pixel_count_per_quarter_panel_row]
              [AC::constants::byte_count_per_quarter_panel_row_grayscale];
};

struct Panel {
  QuarterPanel quarter_panels[AC::constants::quarter_panel_count_per_panel_row]
                              [AC::constants::quarter_panel_count_per_panel_col];
};

struct Region {
  Panel panels[AC::constants::panel_count_per_region_row]
              [AC::constants::panel_count_per_region_col];
};

struct DecodedFrame {
  Region regions[AC::constants::region_count_per_frame];
};

class SpiManager {
 public:
  void begin();

  // Decode a raw pattern frame buffer into the internal decoded_frame_ structure.
  uint16_t decodePatternFrame(const uint8_t *pattern_buf, bool grayscale);

  // Fill the SPI transfer buffer from decoded_frame_.
  void fillBufferFromDecoded(uint8_t *buffer, bool grayscale);

  // Fill the SPI transfer buffer with all-on pattern.
  void fillBufferAllOn(uint8_t *buffer, bool grayscale);

  // Transfer a complete frame buffer to all panels via SPI.
  void transferFrame(const uint8_t *buffer, bool grayscale);

  // Display refresh timer
  void armRefreshTimer(uint32_t frequency_hz);
  void disarmRefreshTimer();

  // Flag set by refresh timer ISR — checked in main loop.
  volatile bool refreshFlag = false;

 private:
  SPIClass *region_spi_[AC::constants::region_count_per_frame]
      = { &SPI, &SPI1 };

  IntervalTimer refreshTimer_;
  DecodedFrame decoded_frame_;

  static SpiManager *instance_;
  static void refreshISR();

  // DMA completion for parallel SPI transfers
  EventResponder dmaEvent_;
  static volatile bool dmaComplete_;
  static void dmaISR(EventResponderRef event);

  void enablePanelSelect(uint8_t row, uint8_t col);
  void disablePanelSelect(uint8_t row, uint8_t col);
  void transferPanelSet(const uint8_t *buffer, uint16_t &pos,
                        uint8_t panel_byte_count);

  static uint8_t remapColumnIndex(uint8_t col);
};
