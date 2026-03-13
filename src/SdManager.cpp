#include "SdManager.h"

using namespace AC;
using namespace AC::constants;

bool SdManager::begin() {
  return sd_.begin(SdioConfig(FIFO_SDIO));
}

bool SdManager::openPatternDirectory() {
  return pattern_dir_.open(pattern_dir_str);
}

uint64_t SdManager::openPatternFile(uint16_t pattern_id) {
  if (!pattern_dir_ || !pattern_dir_.isDir()) return 0;

  pattern_file_.close();

  // Open by directory entry index (matching ArenaController convention)
  uint32_t dir_index = (pattern_id - 1) * 2 + 3;
  bool ok = pattern_file_.open(&pattern_dir_, dir_index, O_RDONLY);
  if (!ok) return 0;
  if (pattern_file_.isDir()) {
    pattern_file_.close();
    return 0;
  }

  return pattern_file_.fileSize();
}

void SdManager::closePatternFile() {
  pattern_file_.close();
}

PatternHeader SdManager::rewindAndReadHeader() {
  pattern_file_.rewind();
  pattern_file_.read(&header_, pattern_header_size);
  return header_;
}

void SdManager::readFrameFromFile(uint8_t *buffer, uint16_t frame_index,
                                  uint64_t byte_count_per_frame) {
  uint32_t file_position = pattern_header_size
                           + frame_index * byte_count_per_frame;
  pattern_file_.seek(file_position);
  pattern_file_.read(buffer, byte_count_per_frame);
}

uint64_t SdManager::byteCountPerFrameGrayscale() const {
  return (uint64_t)byte_count_per_panel_grayscale
             * panel_count_per_frame_row * panel_count_per_frame_col
         + (uint64_t)pattern_row_signifier_byte_count_per_row
               * panel_count_per_frame_row;
}

uint64_t SdManager::byteCountPerFrameBinary() const {
  return (uint64_t)byte_count_per_panel_binary
             * panel_count_per_frame_row * panel_count_per_frame_col
         + (uint64_t)pattern_row_signifier_byte_count_per_row
               * panel_count_per_frame_row;
}
