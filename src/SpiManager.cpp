#include "SpiManager.h"

using namespace AC::constants;

SpiManager *SpiManager::instance_ = nullptr;
volatile bool SpiManager::dmaComplete_ = false;

void SpiManager::dmaISR(EventResponderRef) {
  dmaComplete_ = true;
}

void SpiManager::begin() {
  instance_ = this;

  // Set up DMA completion event for parallel SPI transfers
  dmaEvent_.attachImmediate(dmaISR);

  // Initialize SPI buses
  for (uint8_t r = 0; r < region_count_per_frame; ++r) {
    pinMode(region_cipo_pins[r], INPUT);
    region_spi_[r]->begin();
  }

  // Initialize all panel select pins HIGH (deselected)
  for (uint8_t col = 0; col < panel_count_per_region_col_max; ++col) {
    for (uint8_t row = 0; row < panel_count_per_region_row_max; ++row) {
      pinMode(panel_set_select_pins[row][col], OUTPUT);
      digitalWriteFast(panel_set_select_pins[row][col], HIGH);
    }
  }

  // Reset pin
  pinMode(reset_pin, OUTPUT);
  digitalWriteFast(reset_pin, LOW);
}

void SpiManager::armRefreshTimer(uint32_t frequency_hz) {
  uint32_t period_us = microseconds_per_second / frequency_hz;
  refreshTimer_.begin(refreshISR, period_us);
}

void SpiManager::disarmRefreshTimer() {
  refreshTimer_.end();
}

void SpiManager::refreshISR() {
  if (instance_) instance_->refreshFlag = true;
}

uint8_t SpiManager::remapColumnIndex(uint8_t col) {
  if (col == 0) return 0;
  return panel_count_per_frame_col - col;
}

void SpiManager::enablePanelSelect(uint8_t row, uint8_t col) {
  for (uint8_t r = 0; r < region_count_per_frame; ++r) {
    region_spi_[r]->beginTransaction(
        SPISettings(spi_clock_speed, spi_bit_order, spi_data_mode));
  }
  digitalWriteFast(panel_set_select_pins[row][col], LOW);
}

void SpiManager::disablePanelSelect(uint8_t row, uint8_t col) {
  digitalWriteFast(panel_set_select_pins[row][col], HIGH);
  for (uint8_t r = 0; r < region_count_per_frame; ++r) {
    region_spi_[r]->endTransaction();
  }
}

void SpiManager::transferPanelSet(const uint8_t *buffer, uint16_t &pos,
                                  uint8_t panel_byte_count) {
  // Start async DMA transfer on region 0 (SPI)
  dmaComplete_ = false;
  region_spi_[0]->transfer(buffer + pos, nullptr, panel_byte_count, dmaEvent_);

  // Blocking transfer on region 1 (SPI1) runs simultaneously
  region_spi_[1]->transfer(buffer + pos + panel_byte_count, nullptr,
                           panel_byte_count);

  // Wait for region 0 DMA to complete
  while (!dmaComplete_) { /* spin */ }

  pos += panel_byte_count * region_count_per_frame;
}

void SpiManager::transferFrame(const uint8_t *buffer, bool grayscale) {
  uint32_t t0 = micros();
  uint8_t panel_byte_count = grayscale
      ? byte_count_per_panel_grayscale
      : byte_count_per_panel_binary;
  uint16_t pos = 0;

  for (uint8_t col = 0; col < panel_count_per_region_col; ++col) {
    for (uint8_t row = 0; row < panel_count_per_region_row; ++row) {
      enablePanelSelect(row, col);
      transferPanelSet(buffer, pos, panel_byte_count);
      disablePanelSelect(row, col);
    }
  }
  dbg_transfer_us = micros() - t0;
}

uint16_t SpiManager::decodePatternFrame(const uint8_t *pattern_buf,
                                        bool grayscale) {
  uint8_t bpr = grayscale
      ? byte_count_per_quarter_panel_row_grayscale
      : byte_count_per_quarter_panel_row_binary;

  uint16_t pos = 0;

  for (int8_t fpr = panel_count_per_frame_row - 1; fpr >= 0; --fpr) {
    for (uint8_t qpc = 0; qpc < quarter_panel_count_per_panel_col; ++qpc) {
      for (int8_t qpr = quarter_panel_count_per_panel_row - 1; qpr >= 0; --qpr) {
        // Skip row signifier
        ++pos;

        // Read stretch bytes for all panels in this row
        for (uint8_t fpc = 0; fpc < panel_count_per_frame_col; ++fpc) {
          uint8_t rpc = remapColumnIndex(fpc);
          uint8_t ri = rpc / panel_count_per_region_col_max;
          uint8_t rc = rpc - ri * panel_count_per_region_col_max;

          decoded_frame_.regions[ri].panels[fpr][rc]
              .quarter_panels[qpr][qpc].stretch = pattern_buf[pos++];
        }

        // Read pixel data rows
        for (int8_t pxr = pixel_count_per_quarter_panel_row - 1; pxr >= 0; --pxr) {
          for (uint8_t bi = 0; bi < bpr; ++bi) {
            for (uint8_t fpc = 0; fpc < panel_count_per_frame_col; ++fpc) {
              uint8_t rpc = remapColumnIndex(fpc);
              uint8_t ri = rpc / panel_count_per_region_col_max;
              uint8_t rc = rpc - ri * panel_count_per_region_col_max;

              decoded_frame_.regions[ri].panels[fpr][rc]
                  .quarter_panels[qpr][qpc].data[pxr][bi] = pattern_buf[pos++];
            }
          }
        }
      }
    }
  }
  return pos;
}

void SpiManager::fillBufferFromDecoded(uint8_t *buffer, bool grayscale) {
  uint8_t bpr = grayscale
      ? byte_count_per_quarter_panel_row_grayscale
      : byte_count_per_quarter_panel_row_binary;

  uint16_t pos = 0;
  for (uint8_t rc = 0; rc < panel_count_per_region_col; ++rc) {
    for (uint8_t rr = 0; rr < panel_count_per_region_row; ++rr) {
      for (uint8_t ri = 0; ri < region_count_per_frame; ++ri) {
        for (uint8_t qpc = 0; qpc < quarter_panel_count_per_panel_col; ++qpc) {
          for (uint8_t qpr = 0; qpr < quarter_panel_count_per_panel_row; ++qpr) {
            QuarterPanel &qp = decoded_frame_.regions[ri]
                .panels[rr][rc].quarter_panels[qpr][qpc];
            buffer[pos++] = qp.stretch;
            for (uint8_t pxr = 0; pxr < pixel_count_per_quarter_panel_row; ++pxr) {
              for (uint8_t bi = 0; bi < bpr; ++bi) {
                buffer[pos++] = qp.data[pxr][bi];
              }
            }
          }
        }
      }
    }
  }
}

void SpiManager::fillBufferAllOn(uint8_t *buffer, bool grayscale) {
  uint8_t bpr = grayscale
      ? byte_count_per_quarter_panel_row_grayscale
      : byte_count_per_quarter_panel_row_binary;
  uint8_t stretch = grayscale ? 1 : 50;

  uint16_t pos = 0;
  for (uint8_t rc = 0; rc < panel_count_per_region_col; ++rc) {
    for (uint8_t rr = 0; rr < panel_count_per_region_row; ++rr) {
      for (uint8_t ri = 0; ri < region_count_per_frame; ++ri) {
        for (uint8_t qpc = 0; qpc < quarter_panel_count_per_panel_col; ++qpc) {
          for (uint8_t qpr = 0; qpr < quarter_panel_count_per_panel_row; ++qpr) {
            buffer[pos++] = stretch;
            for (uint8_t pxr = 0; pxr < pixel_count_per_quarter_panel_row; ++pxr) {
              for (uint8_t bi = 0; bi < bpr; ++bi) {
                buffer[pos++] = 255;
              }
            }
          }
        }
      }
    }
  }
}
