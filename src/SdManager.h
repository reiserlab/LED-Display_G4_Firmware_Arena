#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include "constants.h"
#include "PatternHeader.h"

class SdManager {
 public:
  bool begin();

  // Pattern file operations
  bool openPatternDirectory();
  uint64_t openPatternFile(uint16_t pattern_id);
  void closePatternFile();
  AC::PatternHeader rewindAndReadHeader();
  void readFrameFromFile(uint8_t *buffer, uint16_t frame_index,
                         uint64_t byte_count_per_frame);

  // Byte count per pattern frame (includes row signifiers)
  uint64_t byteCountPerFrameGrayscale() const;
  uint64_t byteCountPerFrameBinary() const;

 private:
  SdFs sd_;
  FsFile pattern_dir_;
  FsFile pattern_file_;
  AC::PatternHeader header_;
};
