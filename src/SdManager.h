#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include "constants.h"
#include "PatternHeader.h"

class SdManager {
 public:
  bool begin();

  // Pattern file operations
  uint64_t openPatternFile(uint16_t pattern_id);
  void closePatternFile();
  AC::PatternHeader rewindAndReadHeader();
  void readFrameFromFile(uint8_t *buffer, uint16_t frame_index,
                         uint64_t byte_count_per_frame);

  // Byte count per pattern frame (includes row signifiers)
  uint64_t byteCountPerFrameGrayscale() const;
  uint64_t byteCountPerFrameBinary() const;

  uint16_t patternFileCount() const { return pattern_file_count_; }

 private:
  static constexpr uint16_t max_pattern_files = 10000;
  static constexpr uint8_t sort_key_length = 32;

  bool scanPatternDirectory();

  SdFs sd_;
  FsFile pattern_dir_;
  FsFile pattern_file_;
  AC::PatternHeader header_;

  uint32_t pattern_dir_indices_[max_pattern_files];
  uint16_t pattern_file_count_ = 0;
};
