#pragma once
#include <stdint.h>

namespace AC {

enum ControlModes : uint8_t {
  PLAY_PATTERN_MODE       = 0x02,
  SHOW_PATTERN_FRAME_MODE = 0x03,
  ANALOG_CLOSED_LOOP_MODE = 0x04,
};

} // namespace AC
