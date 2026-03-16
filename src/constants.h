#pragma once
#include <Arduino.h>

// Define DEBUG_SERIAL to enable Serial.printf diagnostics.
// When undefined, all debug prints and their formatting are compiled out.
#ifdef DEBUG_SERIAL
  #define DBG_PRINTF(...) do { Serial.printf("[%lu] ", millis()); Serial.printf(__VA_ARGS__); } while(0)
#else
  #define DBG_PRINTF(...) ((void)0)
#endif

namespace AC {
namespace constants {

// Quarter panel pixels
constexpr uint8_t pixel_count_per_quarter_panel_row = 8;
constexpr uint8_t pixel_count_per_quarter_panel_col = 8;
constexpr uint8_t pixel_count_per_quarter_panel
    = pixel_count_per_quarter_panel_row * pixel_count_per_quarter_panel_col;

// Bits/bytes per pixel
constexpr uint8_t bit_count_per_byte = 8;
constexpr uint8_t bit_count_per_pixel_grayscale = 4;
constexpr uint8_t pixel_count_per_byte_grayscale
    = bit_count_per_byte / bit_count_per_pixel_grayscale;
constexpr uint8_t byte_count_per_quarter_panel_row_grayscale
    = pixel_count_per_quarter_panel_row / pixel_count_per_byte_grayscale;

constexpr uint8_t bit_count_per_pixel_binary = 1;
constexpr uint8_t pixel_count_per_byte_binary
    = bit_count_per_byte / bit_count_per_pixel_binary;
constexpr uint8_t byte_count_per_quarter_panel_row_binary
    = pixel_count_per_quarter_panel_row / pixel_count_per_byte_binary;

// Quarter panel SPI message bytes (1 control + pixel data)
constexpr uint8_t byte_count_per_quarter_panel_control = 1;
constexpr uint8_t byte_count_per_quarter_panel_grayscale
    = byte_count_per_quarter_panel_control
      + pixel_count_per_quarter_panel / pixel_count_per_byte_grayscale; // 33
constexpr uint8_t byte_count_per_quarter_panel_binary
    = byte_count_per_quarter_panel_control
      + pixel_count_per_quarter_panel / pixel_count_per_byte_binary; // 9

// Panel (2x2 quarter panels)
constexpr uint8_t quarter_panel_count_per_panel_row = 2;
constexpr uint8_t quarter_panel_count_per_panel_col = 2;
constexpr uint8_t quarter_panel_count_per_panel
    = quarter_panel_count_per_panel_row * quarter_panel_count_per_panel_col;

constexpr uint8_t byte_count_per_panel_grayscale
    = byte_count_per_quarter_panel_grayscale * quarter_panel_count_per_panel; // 132
constexpr uint8_t byte_count_per_panel_binary
    = byte_count_per_quarter_panel_binary * quarter_panel_count_per_panel; // 36

// Frame geometry
constexpr uint8_t panel_count_per_frame_row_max = 5;
constexpr uint8_t panel_count_per_frame_col_max = 12;
constexpr uint8_t panel_count_per_frame_max
    = panel_count_per_frame_row_max * panel_count_per_frame_col_max;
constexpr uint16_t byte_count_per_frame_max
    = panel_count_per_frame_max * byte_count_per_panel_grayscale; // 7920

// Actual arena dimensions (configurable per arena)
constexpr uint8_t panel_count_per_frame_row = 2;
constexpr uint8_t panel_count_per_frame_col = 12;

// Regions (SPI buses)
constexpr uint8_t region_count_per_frame = 2;
constexpr uint8_t panel_count_per_region_row_max = panel_count_per_frame_row_max;
constexpr uint8_t panel_count_per_region_col_max
    = panel_count_per_frame_col_max / region_count_per_frame; // 6
constexpr uint8_t panel_count_per_region_row = panel_count_per_frame_row;
constexpr uint8_t panel_count_per_region_col
    = panel_count_per_frame_col / region_count_per_frame; // 6

// Panel select pins [row][col]
constexpr uint8_t panel_set_select_pins[panel_count_per_region_row_max]
                                       [panel_count_per_region_col_max]
    = { { 0, 6, 24, 31, 20, 39 },
        { 2, 7, 25, 32, 17, 38 },
        { 3, 8, 28, 23, 16, 37 },
        { 4, 9, 29, 22, 41, 36 },
        { 5, 10, 30, 21, 40, 35 } };

// SPI
constexpr uint32_t spi_clock_speed = 5000000;
constexpr uint8_t spi_bit_order = MSBFIRST;
constexpr uint8_t spi_data_mode = 0x00;  // SPI_MODE0
constexpr uint8_t region_cipo_pins[region_count_per_frame] = { 12, 1 };
constexpr uint8_t reset_pin = 34;

// Grayscale/binary identifiers
constexpr uint8_t pattern_grayscale_value = 16;
constexpr uint8_t pattern_binary_value = 2;
constexpr uint8_t set_grayscale_command_value_grayscale = 1;
constexpr uint8_t set_grayscale_command_value_binary = 0;

// Pattern files
constexpr char pattern_dir_str[] = "/patterns/";
constexpr uint8_t pattern_header_size = 7;
constexpr uint8_t pattern_row_signifier_byte_count_per_row
    = quarter_panel_count_per_panel; // 4

constexpr uint16_t byte_count_per_pattern_frame_max
    = byte_count_per_frame_max
      + pattern_row_signifier_byte_count_per_row * panel_count_per_frame_row_max
      + 7; // 7947

// Byte count per pattern frame (includes row signifiers) — compile-time constants
constexpr uint64_t byte_count_per_pattern_frame_grayscale
    = (uint64_t)byte_count_per_panel_grayscale
          * panel_count_per_frame_row * panel_count_per_frame_col
      + (uint64_t)pattern_row_signifier_byte_count_per_row
          * panel_count_per_frame_row;
constexpr uint64_t byte_count_per_pattern_frame_binary
    = (uint64_t)byte_count_per_panel_binary
          * panel_count_per_frame_row * panel_count_per_frame_col
      + (uint64_t)pattern_row_signifier_byte_count_per_row
          * panel_count_per_frame_row;

// Display refresh defaults
constexpr uint32_t refresh_rate_grayscale_default = 300;
constexpr uint32_t refresh_rate_binary_default = 1000;

// Ethernet
constexpr uint16_t ethernet_server_port = 62222;
constexpr uint8_t stream_header_byte_count = 7;
constexpr uint8_t first_command_byte_max_value_binary = 0x32;
constexpr uint16_t byte_count_per_response_max = 200;

// Timing
constexpr uint32_t microseconds_per_second = 1000000;
constexpr uint32_t milliseconds_per_second = 1000;
constexpr uint32_t milliseconds_per_runtime_duration_unit = 100;

// Analog
constexpr uint16_t analog_output_zero = 0;
constexpr uint16_t analog_output_min = 100;
constexpr uint16_t analog_output_max = 4095;
constexpr uint32_t analog_closed_loop_frequency_hz = 200;
constexpr int32_t analog_closed_loop_offset = 0;
constexpr int32_t analog_closed_loop_scale_factor = 500;

} // namespace constants
} // namespace AC
