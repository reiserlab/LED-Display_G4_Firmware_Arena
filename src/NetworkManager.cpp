#include "NetworkManager.h"
#include "commands.h"

void NetworkManager::begin() {
  Ethernet.begin();  // DHCP
  server_.begin();
}

void NetworkManager::serviceTcp() {
  if (!client_ || !client_.connected()) {
    rx_len_ = 0;
    cmd_ready_ = false;
    EthernetClient newClient = server_.accept();
    if (newClient) {
      client_ = newClient;
      client_.setNoDelay(true);
    }
  }

  // Cache IP once available
  if (ip_str_[0] == '\0') {
    IPAddress ip = Ethernet.localIP();
    if (ip != IPAddress{0, 0, 0, 0}) {
      snprintf(ip_str_, sizeof(ip_str_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
  }

  parseIncoming();
}

void NetworkManager::parseIncoming() {
  if (!client_ || !client_.connected()) return;
  if (cmd_ready_) return;  // previous command not consumed yet

  dbg_frame_complete = false;
  ++dbg_parse_attempts;

  // Read available bytes into rx_buf_ (bulk read)
  size_t rx_len_before = rx_len_;
  uint32_t t_read_start = micros();
  int avail = client_.available();
  if (avail > 0) {
    size_t to_read = min((size_t)avail, RX_BUF_SIZE - rx_len_);
    int n = client_.read(rx_buf_ + rx_len_, to_read);
    if (n > 0) rx_len_ += n;
  }
  dbg_read_us = micros() - t_read_start;
  dbg_read_bytes = rx_len_ - rx_len_before;

  if (rx_len_ == 0) return;

  // G4 protocol: byte 0 = length of remaining bytes
  // For binary commands: [len, cmd, params...]
  // For stream command (0x32): [cmd=0x32, len_lo, len_hi, ...data...]
  uint8_t first_byte = rx_buf_[0];

  if (first_byte == AC::STREAM_FRAME_CMD) {
    // Stream command: byte0=cmd(0x32), bytes1-2=data_length (LE)
    if (rx_len_ < 3) return;  // need at least cmd + 2-byte length
    uint16_t claimed_len;
    memcpy(&claimed_len, rx_buf_ + 1, sizeof(claimed_len));
    uint16_t total_needed = AC::constants::stream_header_byte_count + claimed_len;  // 7-byte header + frame_data
    if (rx_len_ < total_needed) return;  // wait for more data

    parsed_cmd_.cmd = AC::STREAM_FRAME_CMD;
    parsed_cmd_.is_stream = true;
    parsed_cmd_.data_len = total_needed;
    uint32_t t_cpy = micros();
    memcpy(parsed_cmd_.data, rx_buf_, total_needed);
    dbg_memcpy_us = micros() - t_cpy;
    cmd_ready_ = true;
    dbg_frame_complete = true;

    // Shift remaining bytes
    size_t consumed = total_needed;
    if (consumed < rx_len_) {
      memmove(rx_buf_, rx_buf_ + consumed, rx_len_ - consumed);
    }
    rx_len_ -= consumed;
  } else if (first_byte <= AC::constants::first_command_byte_max_value_binary) {
    // Binary command: byte0 = length of remaining bytes, byte1 = cmd
    uint8_t remaining_len = first_byte;
    uint16_t total_needed = 1 + remaining_len;
    if (rx_len_ < total_needed) return;

    parsed_cmd_.is_stream = false;
    parsed_cmd_.data_len = total_needed;
    memcpy(parsed_cmd_.data, rx_buf_, total_needed);
    parsed_cmd_.cmd = rx_buf_[1];
    cmd_ready_ = true;

    if (total_needed < rx_len_) {
      memmove(rx_buf_, rx_buf_ + total_needed, rx_len_ - total_needed);
    }
    rx_len_ -= total_needed;
  } else {
    // Unknown/string command — discard
    rx_len_ = 0;
  }
}

void NetworkManager::flushResponses() {
  if (!client_ || !client_.connected()) return;
  if (resp_len_ == 0) { dbg_flush_us = 0; return; }

  uint32_t t0 = micros();
  client_.write(resp_buf_, resp_len_);
  client_.flush();
  dbg_flush_us = micros() - t0;
  resp_len_ = 0;
}

void NetworkManager::sendResponse(uint8_t cmd_echo, uint8_t status,
                                  const char *message) {
  size_t msg_len = strlen(message);
  uint8_t response_byte_count = 0;
  resp_buf_[response_byte_count++] = 2;  // placeholder length
  resp_buf_[response_byte_count++] = status;
  resp_buf_[response_byte_count++] = cmd_echo;
  if (msg_len > 0 && (response_byte_count + msg_len) < RESP_BUF_SIZE) {
    memcpy(resp_buf_ + response_byte_count, message, msg_len);
    response_byte_count += msg_len;
  }
  resp_buf_[0] = response_byte_count - 1;  // length excluding length byte
  resp_len_ = response_byte_count;
}

void NetworkManager::sendRaw(const uint8_t *data, size_t len) {
  if (len > RESP_BUF_SIZE) return;
  memcpy(resp_buf_, data, len);
  resp_len_ = len;
}
