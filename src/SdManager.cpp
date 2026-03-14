#include "SdManager.h"
#include <cstring>

using namespace AC;
using namespace AC::constants;

bool SdManager::begin() {
  if (!sd_.begin(SdioConfig(FIFO_SDIO))) return false;
  return scanPatternDirectory();
}

bool SdManager::scanPatternDirectory() {
  pattern_file_count_ = 0;

  // Heap-allocate sort keys; freed after sorting
  auto sort_keys = new char[max_pattern_files][sort_key_length];
  if (!sort_keys) return false;

  FsFile dir;
  if (!dir.open(pattern_dir_str)) { delete[] sort_keys; return false; }

  FsFile entry;
  char name_buf[sort_key_length];
  while (entry.openNext(&dir, O_RDONLY)) {
    entry.getName(name_buf, sizeof(name_buf));
    if (entry.isDir() || name_buf[0] == '.') {
      entry.close();
      continue;
    }
    if (pattern_file_count_ >= max_pattern_files) {
      entry.close();
      break;
    }
    strncpy(sort_keys[pattern_file_count_], name_buf, sort_key_length);
    pattern_dir_indices_[pattern_file_count_] = entry.dirIndex();
    entry.close();
    pattern_file_count_++;
  }
  dir.close();

  // Insertion sort alphabetically so pattern_id mapping is deterministic
  char tmp_key[sort_key_length];
  uint32_t tmp_idx;
  for (uint16_t i = 1; i < pattern_file_count_; i++) {
    if (memcmp(sort_keys[i], sort_keys[i - 1], sort_key_length) < 0) {
      memcpy(tmp_key, sort_keys[i], sort_key_length);
      tmp_idx = pattern_dir_indices_[i];
      uint16_t j = i;
      do {
        memcpy(sort_keys[j], sort_keys[j - 1], sort_key_length);
        pattern_dir_indices_[j] = pattern_dir_indices_[j - 1];
        j--;
      } while (j > 0 && memcmp(tmp_key, sort_keys[j - 1], sort_key_length) < 0);
      memcpy(sort_keys[j], tmp_key, sort_key_length);
      pattern_dir_indices_[j] = tmp_idx;
    }
  }

  delete[] sort_keys;
  return true;
}

uint64_t SdManager::openPatternFile(uint16_t pattern_id) {
  // pattern_id is 1-based
  if (pattern_id == 0 || pattern_id > pattern_file_count_) return 0;

  pattern_file_.close();

  FsFile dir;
  if (!dir.open(pattern_dir_str)) return 0;

  if (!pattern_file_.open(&dir, pattern_dir_indices_[pattern_id - 1], O_RDONLY)) {
    dir.close();
    return 0;
  }
  dir.close();

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
