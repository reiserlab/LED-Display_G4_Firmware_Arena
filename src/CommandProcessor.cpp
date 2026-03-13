#include "CommandProcessor.h"

using namespace AC;
using namespace AC::constants;

void CommandProcessor::begin() {
  // No-op; initialization done in constructors
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------

void CommandProcessor::processCommand() {
  if (!net_.hasCommand()) return;

  const ParsedCommand &cmd = net_.command();
  if (cmd.is_stream) {
    handleStreamCommand(cmd);
  } else {
    handleBinaryCommand(cmd);
  }
  net_.commandConsumed();
}

void CommandProcessor::handleBinaryCommand(const ParsedCommand &cmd) {
  const uint8_t *buf = cmd.data;
  uint8_t claimed_len = buf[0];
  if (cmd.data_len - 1 != claimed_len) {
    // Malformed command
    return;
  }

  uint8_t pos = 1;
  uint8_t command_byte = buf[pos++];

  switch (command_byte) {
    case ALL_OFF_CMD:
      enterAllOff();
      net_.sendResponse(command_byte, 0, "All-Off Received");
      break;

    case ALL_ON_CMD:
      enterAllOn();
      net_.sendResponse(command_byte, 0, "All-On Received");
      break;

    case STOP_DISPLAY_CMD:
      enterAllOff();
      net_.sendResponse(command_byte, 0, "Display has been stopped");
      break;

    case DISPLAY_RESET_CMD:
      net_.sendResponse(command_byte, 0, "Reset Command Sent to FPGA");
      break;

    case SWITCH_GRAYSCALE_CMD: {
      uint8_t gs_val = buf[pos];
      enterAllOff();
      if (gs_val == set_grayscale_command_value_grayscale) {
        grayscale_ = true;
        refresh_rate_hz_ = refresh_rate_grayscale_default;
      } else if (gs_val == set_grayscale_command_value_binary) {
        grayscale_ = false;
        refresh_rate_hz_ = refresh_rate_binary_default;
      }
      net_.sendResponse(command_byte, 0, "");
      break;
    }

    case SET_REFRESH_RATE_CMD: {
      uint16_t rate;
      memcpy(&rate, buf + pos, sizeof(rate));
      refresh_rate_hz_ = rate;
      // If display is active, re-arm timer at new rate
      if (state_ != ArenaState::ALL_OFF) {
        spi_.armRefreshTimer(refresh_rate_hz_);
      }
      net_.sendResponse(command_byte, 0, "");
      break;
    }

    case TRIAL_PARAMS_CMD: {
      uint8_t mode;
      memcpy(&mode, buf + pos, sizeof(mode)); pos += sizeof(mode);

      uint16_t pid;
      memcpy(&pid, buf + pos, sizeof(pid)); pos += sizeof(pid);

      int16_t frame_rate;
      memcpy(&frame_rate, buf + pos, sizeof(frame_rate)); pos += sizeof(frame_rate);

      uint16_t fidx;
      memcpy(&fidx, buf + pos, sizeof(fidx)); pos += sizeof(fidx);

      uint16_t gain;
      memcpy(&gain, buf + pos, sizeof(gain)); pos += sizeof(gain);

      uint16_t runtime_dur;
      memcpy(&runtime_dur, buf + pos, sizeof(runtime_dur));

      switch (mode) {
        case PLAY_PATTERN_MODE:
          enterPlayPattern(pid, frame_rate, fidx, runtime_dur);
          break;
        case SHOW_PATTERN_FRAME_MODE:
          enterShowPatternFrame(pid, fidx);
          break;
        case ANALOG_CLOSED_LOOP_MODE:
          enterAnalogClosedLoop(pid, gain, fidx, runtime_dur);
          break;
      }
      net_.sendResponse(command_byte, 0, "");
      break;
    }

    case GET_ETHERNET_IP_ADDRESS_CMD:
      net_.sendResponse(command_byte, 0, net_.ipAddress());
      break;

    case SET_FRAME_POSITION_CMD: {
      uint16_t fidx;
      memcpy(&fidx, buf + pos, sizeof(fidx));
      if (state_ == ArenaState::SHOWING_PATTERN_FRAME
          || state_ == ArenaState::PLAYING_PATTERN
          || state_ == ArenaState::ANALOG_CLOSED_LOOP) {
        frame_index_ = fidx;
        if (frame_index_ >= frame_count_) frame_index_ = 0;
        readDecodeAndFillFrame();
      }
      net_.sendResponse(command_byte, 0, "");
      break;
    }

    default:
      break;
  }
}

void CommandProcessor::handleStreamCommand(const ParsedCommand &cmd) {
  const uint8_t *buf = cmd.data;
  // buf[0] = 0x32, buf[1..2] = data_length, buf[3..4] = analog_x, buf[5..6] = analog_y
  // buf[7..] = frame data

  const uint8_t *frame_data = buf + stream_header_byte_count;
  uint32_t frame_byte_count = cmd.data_len - stream_header_byte_count;

  bool gs;
  if (frame_byte_count == sd_.byteCountPerFrameGrayscale()) {
    gs = true;
  } else if (frame_byte_count == sd_.byteCountPerFrameBinary()) {
    gs = false;
  } else {
    enterAllOff();
    net_.sendResponse(STREAM_FRAME_CMD, 0, "");
    return;
  }

  grayscale_ = gs;
  dbg_frame_bytes = frame_byte_count;

  // Decode and fill frame buffer
  uint32_t t0 = micros();
  spi_.decodePatternFrame(frame_data, grayscale_);
  uint32_t t1 = micros();
  spi_.fillBufferFromDecoded(frame_buf_, grayscale_);
  uint32_t t2 = micros();

  dbg_decode_us = t1 - t0;
  dbg_fill_us = t2 - t1;
  ++dbg_stream_count;

  // Transition to streaming
  if (state_ != ArenaState::STREAMING_FRAME) {
    spi_.disarmRefreshTimer();
    state_ = ArenaState::STREAMING_FRAME;
    spi_.armRefreshTimer(refresh_rate_hz_);
  }

  net_.sendResponse(STREAM_FRAME_CMD, 0, "");
}

// ---------------------------------------------------------------------------
// Display service — called every loop iteration
// ---------------------------------------------------------------------------

void CommandProcessor::serviceDisplay() {
  switch (state_) {
    case ArenaState::ALL_OFF:
      break;

    case ArenaState::ALL_ON:
    case ArenaState::STREAMING_FRAME:
    case ArenaState::SHOWING_PATTERN_FRAME:
      // Transfer frame at refresh rate
      if (spi_.refreshFlag) {
        spi_.refreshFlag = false;
        spi_.transferFrame(frame_buf_, grayscale_);
      }
      break;

    case ArenaState::PLAYING_PATTERN:
      // Transfer frame at refresh rate
      if (spi_.refreshFlag) {
        spi_.refreshFlag = false;
        spi_.transferFrame(frame_buf_, grayscale_);
      }
      // Advance frame at frame rate
      if (frame_period_ms_ > 0 && frame_rate_elapsed_ >= frame_period_ms_) {
        frame_rate_elapsed_ -= frame_period_ms_;
        advanceFrameIndex();
        readDecodeAndFillFrame();
      }
      // Check runtime duration
      if (runtime_duration_ms_ > 0 && runtime_elapsed_ >= runtime_duration_ms_) {
        net_.sendResponse(TRIAL_PARAMS_CMD, 0, "");
        enterAllOff();
      }
      break;

    case ArenaState::ANALOG_CLOSED_LOOP:
      // Transfer frame at refresh rate
      if (spi_.refreshFlag) {
        spi_.refreshFlag = false;
        spi_.transferFrame(frame_buf_, grayscale_);
      }
      // Advance frame at closed-loop rate
      if (acl_elapsed_ >= ACL_PERIOD_MS) {
        acl_elapsed_ -= ACL_PERIOD_MS;
        ++timeout_count_;
        if (timeout_counts_per_frame_index_ > 0
            && (timeout_count_ % timeout_counts_per_frame_index_) == 0) {
          advanceFrameIndex();
          readDecodeAndFillFrame();
        }
      }
      // Check runtime duration
      if (runtime_duration_ms_ > 0 && runtime_elapsed_ >= runtime_duration_ms_) {
        net_.sendResponse(TRIAL_PARAMS_CMD, 0, "");
        enterAllOff();
      }
      break;
  }
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

void CommandProcessor::enterAllOff() {
  spi_.disarmRefreshTimer();
  sd_.closePatternFile();
  state_ = ArenaState::ALL_OFF;
}

void CommandProcessor::enterAllOn() {
  spi_.disarmRefreshTimer();
  sd_.closePatternFile();

  spi_.fillBufferAllOn(frame_buf_, grayscale_);
  state_ = ArenaState::ALL_ON;
  spi_.armRefreshTimer(refresh_rate_hz_);
}

bool CommandProcessor::openAndValidatePattern(uint16_t pid) {
  if (!sd_.openPatternDirectory()) return false;

  uint64_t file_size = sd_.openPatternFile(pid);
  if (file_size == 0) return false;

  PatternHeader hdr = sd_.rewindAndReadHeader();

  // Validate dimensions
  if (hdr.panel_count_per_frame_row != panel_count_per_region_row) return false;
  if (hdr.panel_count_per_frame_col
      != panel_count_per_region_col * region_count_per_frame) return false;

  // Determine grayscale/binary
  uint64_t bcpf;
  if (hdr.grayscale_value == pattern_grayscale_value) {
    grayscale_ = true;
    bcpf = sd_.byteCountPerFrameGrayscale();
    refresh_rate_hz_ = refresh_rate_grayscale_default;
  } else if (hdr.grayscale_value == pattern_binary_value) {
    grayscale_ = false;
    bcpf = sd_.byteCountPerFrameBinary();
    refresh_rate_hz_ = refresh_rate_binary_default;
  } else {
    return false;
  }

  // Validate file size
  if ((uint64_t)(hdr.frame_count_x * bcpf + pattern_header_size) != file_size) {
    return false;
  }

  frame_count_ = hdr.frame_count_x;
  byte_count_per_frame_ = bcpf;
  return true;
}

void CommandProcessor::enterPlayPattern(uint16_t pid, int16_t rate,
                                        uint16_t fidx, uint16_t runtime_dur) {
  spi_.disarmRefreshTimer();

  if (rate == 0) { enterAllOff(); return; }

  positive_direction_ = (rate > 0);
  frame_rate_hz_ = abs(rate);
  runtime_duration_ms_ = (uint32_t)runtime_dur * milliseconds_per_runtime_duration_unit;
  frame_index_ = fidx;
  pattern_id_ = pid;

  if (!openAndValidatePattern(pid)) { enterAllOff(); return; }
  if (frame_index_ >= frame_count_) frame_index_ = 0;

  frame_period_ms_ = milliseconds_per_second / frame_rate_hz_;

  readDecodeAndFillFrame();

  state_ = ArenaState::PLAYING_PATTERN;
  frame_rate_elapsed_ = 0;
  runtime_elapsed_ = 0;
  spi_.armRefreshTimer(refresh_rate_hz_);
}

void CommandProcessor::enterShowPatternFrame(uint16_t pid, uint16_t fidx) {
  spi_.disarmRefreshTimer();

  frame_index_ = fidx;
  pattern_id_ = pid;

  if (!openAndValidatePattern(pid)) { enterAllOff(); return; }
  if (frame_index_ >= frame_count_) frame_index_ = 0;

  readDecodeAndFillFrame();

  state_ = ArenaState::SHOWING_PATTERN_FRAME;
  spi_.armRefreshTimer(refresh_rate_hz_);
}

void CommandProcessor::enterAnalogClosedLoop(uint16_t pid, int16_t gain_val,
                                             uint16_t fidx,
                                             uint16_t runtime_dur) {
  spi_.disarmRefreshTimer();

  gain_ = gain_val;
  runtime_duration_ms_ = (uint32_t)runtime_dur * milliseconds_per_runtime_duration_unit;
  frame_index_ = fidx;
  pattern_id_ = pid;
  positive_direction_ = true;
  timeout_count_ = 0;
  timeout_counts_per_frame_index_ = 1;

  if (!openAndValidatePattern(pid)) { enterAllOff(); return; }
  if (frame_index_ >= frame_count_) frame_index_ = 0;

  readDecodeAndFillFrame();

  state_ = ArenaState::ANALOG_CLOSED_LOOP;
  acl_elapsed_ = 0;
  runtime_elapsed_ = 0;
  spi_.armRefreshTimer(refresh_rate_hz_);
}

// ---------------------------------------------------------------------------
// Frame helpers
// ---------------------------------------------------------------------------

void CommandProcessor::readDecodeAndFillFrame() {
  sd_.readFrameFromFile(pattern_frame_buf_, frame_index_, byte_count_per_frame_);
  spi_.decodePatternFrame(pattern_frame_buf_, grayscale_);
  spi_.fillBufferFromDecoded(frame_buf_, grayscale_);
}

void CommandProcessor::advanceFrameIndex() {
  if (positive_direction_) {
    ++frame_index_;
    if (frame_index_ >= frame_count_) frame_index_ = 0;
  } else {
    if (frame_index_ > 0) {
      --frame_index_;
    } else {
      frame_index_ = frame_count_ - 1;
    }
  }
}
