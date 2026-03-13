#pragma once

#include "NetworkManager.h"
#include "SdManager.h"
#include "SpiManager.h"
#include "commands.h"
#include "modes.h"
#include "PatternHeader.h"

enum class ArenaState : uint8_t {
  ALL_OFF,
  ALL_ON,
  PLAYING_PATTERN,
  SHOWING_PATTERN_FRAME,
  STREAMING_FRAME,
  ANALOG_CLOSED_LOOP,
};

class CommandProcessor {
 public:
  CommandProcessor(NetworkManager &net, SdManager &sd, SpiManager &spi)
      : net_(net), sd_(sd), spi_(spi) {}

  void begin();
  void processCommand();
  void serviceDisplay();

  // Debug timing (written by processCommand, read by main loop)
  uint32_t dbg_decode_us = 0;
  uint32_t dbg_fill_us = 0;
  uint32_t dbg_frame_bytes = 0;
  uint32_t dbg_stream_count = 0;

 private:
  NetworkManager &net_;
  SdManager      &sd_;
  SpiManager     &spi_;

  ArenaState state_ = ArenaState::ALL_OFF;
  bool grayscale_ = true;
  uint32_t refresh_rate_hz_ = AC::constants::refresh_rate_grayscale_default;

  // Frame buffer for SPI transfers
  uint8_t frame_buf_[AC::constants::byte_count_per_frame_max];

  // Pattern frame buffer (raw from SD, before decoding)
  uint8_t pattern_frame_buf_[AC::constants::byte_count_per_pattern_frame_max];

  // Pattern playback state
  uint16_t pattern_id_ = 0;
  uint16_t frame_count_ = 0;
  uint16_t frame_index_ = 0;
  uint64_t byte_count_per_frame_ = 0;
  int16_t  frame_rate_hz_ = 0;
  bool     positive_direction_ = true;
  uint32_t runtime_duration_ms_ = 0;

  // Timing
  elapsedMillis frame_rate_elapsed_;
  elapsedMillis runtime_elapsed_;
  uint32_t frame_period_ms_ = 0;

  // Analog closed loop
  int16_t gain_ = 10;
  uint32_t timeout_count_ = 0;
  uint32_t timeout_counts_per_frame_index_ = 1;
  elapsedMillis acl_elapsed_;
  static constexpr uint32_t ACL_PERIOD_MS
      = AC::constants::milliseconds_per_second
        / AC::constants::analog_closed_loop_frequency_hz;

  // Helpers
  void handleBinaryCommand(const ParsedCommand &cmd);
  void handleStreamCommand(const ParsedCommand &cmd);

  bool openAndValidatePattern(uint16_t pid);
  void readDecodeAndFillFrame();
  void advanceFrameIndex();
  void enterAllOff();
  void enterAllOn();
  void enterPlayPattern(uint16_t pid, int16_t rate, uint16_t fidx,
                        uint16_t runtime_dur);
  void enterShowPatternFrame(uint16_t pid, uint16_t fidx);
  void enterAnalogClosedLoop(uint16_t pid, int16_t gain, uint16_t fidx,
                             uint16_t runtime_dur);
};
