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

// ---------------------------------------------------------------------------
// Debug performance stats
// ---------------------------------------------------------------------------
struct PerfStats {
  uint32_t loop_count = 0;
  uint32_t loop_us_total = 0;
  uint32_t loop_us_max = 0;

  // Per-section totals
  uint32_t tcp_us_total = 0;
  uint32_t tcp_us_max = 0;
  uint32_t cmd_us_total = 0;
  uint32_t cmd_us_max = 0;
  uint32_t disp_us_total = 0;
  uint32_t disp_us_max = 0;
  uint32_t flush_us_total = 0;
  uint32_t flush_us_max = 0;

  // Stream frame specifics
  uint32_t stream_count = 0;
  uint32_t decode_us_total = 0;
  uint32_t decode_us_max = 0;
  uint32_t fill_us_total = 0;
  uint32_t fill_us_max = 0;
  uint32_t recv_memcpy_us_total = 0;
  uint32_t parse_attempts_total = 0; // sum of parse_attempts per frame
  uint32_t parse_attempts_max = 0;

  // SPI transfer
  uint32_t xfer_count = 0;
  uint32_t xfer_us_total = 0;
  uint32_t xfer_us_max = 0;

  // TCP flush
  uint32_t flush_write_us_total = 0;
  uint32_t flush_write_us_max = 0;

  // Byte-at-a-time read
  uint32_t read_us_total = 0;
  uint32_t read_us_max = 0;
  uint32_t read_bytes_total = 0;

  void reset() { memset(this, 0, sizeof(*this)); }
};

static PerfStats stats;
static elapsedMillis sincePerfPrint;
static constexpr uint32_t PERF_PRINT_INTERVAL_MS = 1000;
static inline uint32_t maxOf(uint32_t a, uint32_t b) { return a > b ? a : b; }

void printPerfStats() {
  if (!Serial) return;

  uint32_t n = stats.loop_count;
  if (n == 0) return;

  uint32_t stream_fps = stats.stream_count;
  uint32_t xfer_fps = stats.xfer_count;

  Serial.printf("\n[PERF] --- 1s summary --- loops=%lu stream_frames=%lu spi_xfers=%lu\n",
                (unsigned long)n, (unsigned long)stream_fps, (unsigned long)xfer_fps);

  Serial.printf("[PERF] loop: avg=%luus max=%luus\n",
                (unsigned long)(stats.loop_us_total / n),
                (unsigned long)stats.loop_us_max);

  Serial.printf("[PERF] tcp_recv:  avg=%luus max=%luus  read_bytes=%lu  read_avg=%luus read_max=%luus\n",
                (unsigned long)(stats.tcp_us_total / n),
                (unsigned long)stats.tcp_us_max,
                (unsigned long)stats.read_bytes_total,
                (unsigned long)(n > 0 ? stats.read_us_total / n : 0),
                (unsigned long)stats.read_us_max);

  Serial.printf("[PERF] cmd_proc:  avg=%luus max=%luus\n",
                (unsigned long)(stats.cmd_us_total / n),
                (unsigned long)stats.cmd_us_max);

  Serial.printf("[PERF] svc_disp:  avg=%luus max=%luus\n",
                (unsigned long)(stats.disp_us_total / n),
                (unsigned long)stats.disp_us_max);

  Serial.printf("[PERF] flush:     avg=%luus max=%luus  write_avg=%luus write_max=%luus\n",
                (unsigned long)(stats.flush_us_total / n),
                (unsigned long)stats.flush_us_max,
                (unsigned long)(n > 0 ? stats.flush_write_us_total / n : 0),
                (unsigned long)stats.flush_write_us_max);

  if (stats.stream_count > 0) {
    uint32_t s = stats.stream_count;
    Serial.printf("[PERF] stream: frame_bytes=%lu  parse_calls: avg=%.1f max=%lu\n",
                  (unsigned long)cmdProc.dbg_frame_bytes,
                  (float)stats.parse_attempts_total / s,
                  (unsigned long)stats.parse_attempts_max);

    Serial.printf("[PERF] stream: decode: avg=%luus max=%luus  fill: avg=%luus max=%luus  memcpy: avg=%luus\n",
                  (unsigned long)(stats.decode_us_total / s),
                  (unsigned long)stats.decode_us_max,
                  (unsigned long)(stats.fill_us_total / s),
                  (unsigned long)stats.fill_us_max,
                  (unsigned long)(stats.recv_memcpy_us_total / s));
  }

  if (stats.xfer_count > 0) {
    Serial.printf("[PERF] spi_xfer: avg=%luus max=%luus\n",
                  (unsigned long)(stats.xfer_us_total / stats.xfer_count),
                  (unsigned long)stats.xfer_us_max);
  }

  stats.reset();
}

// ---------------------------------------------------------------------------
// Setup & loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  spi.begin();
  sdMgr.begin();
  net.begin();
  cmdProc.begin();

  setupInterruptPriorities();
}

void loop() {
  uint32_t t_loop = micros();

  // 1. Accept client, read and parse commands
  uint32_t t0 = micros();
  net.serviceTcp();
  uint32_t t1 = micros();

  // 2. Handle one parsed command
  uint32_t prev_stream_count = cmdProc.dbg_stream_count;
  cmdProc.processCommand();
  uint32_t t2 = micros();

  // 3. Pattern playback, frame transfers
  uint32_t prev_xfer_us = spi.dbg_transfer_us;
  cmdProc.serviceDisplay();
  uint32_t t3 = micros();

  // 4. Send ready responses over TCP
  net.flushResponses();
  uint32_t t4 = micros();

  // Accumulate stats
  uint32_t tcp_us  = t1 - t0;
  uint32_t cmd_us  = t2 - t1;
  uint32_t disp_us = t3 - t2;
  uint32_t flush_us = t4 - t3;
  uint32_t loop_us = t4 - t_loop;

  stats.loop_count++;
  stats.loop_us_total += loop_us;
  stats.loop_us_max = maxOf(stats.loop_us_max, loop_us);

  stats.tcp_us_total += tcp_us;
  stats.tcp_us_max = maxOf(stats.tcp_us_max, tcp_us);
  stats.read_us_total += net.dbg_read_us;
  stats.read_us_max = maxOf(stats.read_us_max, net.dbg_read_us);
  stats.read_bytes_total += net.dbg_read_bytes;

  stats.cmd_us_total += cmd_us;
  stats.cmd_us_max = maxOf(stats.cmd_us_max, cmd_us);

  stats.disp_us_total += disp_us;
  stats.disp_us_max = maxOf(stats.disp_us_max, disp_us);

  stats.flush_us_total += flush_us;
  stats.flush_us_max = maxOf(stats.flush_us_max, flush_us);

  stats.flush_write_us_total += net.dbg_flush_us;
  stats.flush_write_us_max = maxOf(stats.flush_write_us_max, net.dbg_flush_us);

  // Track stream frame stats
  if (cmdProc.dbg_stream_count != prev_stream_count) {
    stats.stream_count++;
    stats.decode_us_total += cmdProc.dbg_decode_us;
    stats.decode_us_max = maxOf(stats.decode_us_max, cmdProc.dbg_decode_us);
    stats.fill_us_total += cmdProc.dbg_fill_us;
    stats.fill_us_max = maxOf(stats.fill_us_max, cmdProc.dbg_fill_us);
    stats.recv_memcpy_us_total += net.dbg_memcpy_us;
    stats.parse_attempts_total += net.dbg_parse_attempts;
    stats.parse_attempts_max = maxOf(stats.parse_attempts_max, net.dbg_parse_attempts);
    net.dbg_parse_attempts = 0;
  }

  // Track SPI transfer stats
  if (spi.dbg_transfer_us != prev_xfer_us && spi.dbg_transfer_us > 0) {
    stats.xfer_count++;
    stats.xfer_us_total += spi.dbg_transfer_us;
    stats.xfer_us_max = maxOf(stats.xfer_us_max, spi.dbg_transfer_us);
  }

  // Periodic print
  if (sincePerfPrint >= PERF_PRINT_INTERVAL_MS) {
    sincePerfPrint = 0;
    printPerfStats();

    // Also print IP periodically
    if (Serial) {
      Serial.printf("IP: %s\n", net.ipAddress());
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
