#pragma once

#include <Arduino.h>
#include <QNEthernet.h>
#include "constants.h"

using namespace qindesign::network;

// Parsed command from the G4 binary protocol.
struct ParsedCommand {
  uint8_t  cmd;
  uint8_t  data[AC::constants::byte_count_per_pattern_frame_max + 16];
  uint16_t data_len;  // total bytes received (including length byte)
  bool     is_stream;
};

class NetworkManager {
 public:
  void begin();

  void serviceTcp();
  void flushResponses();

  bool hasCommand() const { return cmd_ready_; }
  const ParsedCommand &command() const { return parsed_cmd_; }
  void commandConsumed() { cmd_ready_ = false; }

  // Build and queue a binary response: [len, status, echo_cmd, ...message]
  void sendResponse(uint8_t cmd_echo, uint8_t status, const char *message);
  // Send raw bytes as response
  void sendRaw(const uint8_t *data, size_t len);

  const char *ipAddress() const { return ip_str_; }

  // Debug timing (written by serviceTcp/flushResponses, read by main loop)
  uint32_t dbg_read_us = 0;        // time in byte read loop
  uint32_t dbg_read_bytes = 0;     // bytes read this call
  uint32_t dbg_parse_attempts = 0; // parseIncoming calls since last complete frame
  bool     dbg_frame_complete = false;
  uint32_t dbg_memcpy_us = 0;      // memcpy time on frame completion
  uint32_t dbg_flush_us = 0;       // time in flushResponses (write+flush)

 private:
  EthernetServer server_{AC::constants::ethernet_server_port};
  EthernetClient client_;

  // Receive buffer
  static constexpr size_t RX_BUF_SIZE
      = AC::constants::byte_count_per_pattern_frame_max + 16;
  uint8_t rx_buf_[RX_BUF_SIZE];
  size_t  rx_len_ = 0;

  ParsedCommand parsed_cmd_;
  bool cmd_ready_ = false;

  // Response buffer
  static constexpr size_t RESP_BUF_SIZE = AC::constants::byte_count_per_response_max;
  uint8_t resp_buf_[RESP_BUF_SIZE];
  size_t  resp_len_ = 0;

  char ip_str_[32] = "";

  void parseIncoming();
};
