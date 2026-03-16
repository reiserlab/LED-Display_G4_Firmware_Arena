#include "SdManager.h"
#include <cstring>

using namespace AC;
using namespace AC::constants;

bool SdManager::begin() {
  if (!sd_.begin(SdioConfig(FIFO_SDIO))) return false;
  if (!scanPatternDirectory()) return false;
  if (!pattern_dir_.open(pattern_dir_str)) return false;
  return true;
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
  uint32_t t0 = micros();

  // pattern_id is 1-based
  if (pattern_id == 0 || pattern_id > pattern_file_count_) {
    Serial.printf("[SdManager::openPatternFile] pattern_id=%u OUT OF RANGE (count=%u)\n",
                  pattern_id, pattern_file_count_);
    return 0;
  }

  uint16_t file_index = pattern_id - 1;

  uint32_t t1 = micros();
  free(pattern_cache_);
  pattern_cache_ = nullptr;
  pattern_cache_size_ = 0;
  pattern_file_.close();
  uint32_t t2 = micros();

  if (!pattern_file_.open(&pattern_dir_, pattern_dir_indices_[file_index], O_RDONLY)) {
    return 0;
  }
  uint32_t t3 = micros();

  uint64_t size = pattern_file_.fileSize();
  uint32_t t4 = micros();

  // Try to cache the entire file in RAM.  If malloc succeeds and the
  // read completes, every subsequent access is a pure memcpy.
  // If the file is too large for the heap, fall back to pre-warming
  // the FAT cache so sequential reads don't stall on cluster lookups.
  bool cached = false;
  if (size > 0) {
    uint8_t *buf = (uint8_t *)malloc((uint32_t)size);
    if (buf) {
      pattern_file_.rewind();
      if (pattern_file_.read(buf, (uint32_t)size) == (int32_t)(uint32_t)size) {
        pattern_cache_ = buf;
        pattern_cache_size_ = (uint32_t)size;
        cached = true;
      } else {
        free(buf);
      }
    }

    if (!cached) {
      pattern_file_.seek(size - 1);
      uint8_t dummy;
      pattern_file_.read(&dummy, 1);
      pattern_file_.rewind();
    }
  }
  uint32_t t5 = micros();

  Serial.printf("[SdManager::openPatternFile] pattern_id=%u file_index=%u dir_index=%lu size=%lu cached=%u  close=%lu open=%lu fileSize=%lu load=%lu total=%lu us\n",
                pattern_id, file_index, pattern_dir_indices_[file_index], (uint32_t)size,
                cached, t2 - t1, t3 - t2, t4 - t3, t5 - t4, t5 - t0);
  return size;
}

void SdManager::closePatternFile() {
  uint32_t t0 = micros();
  free(pattern_cache_);
  pattern_cache_ = nullptr;
  pattern_cache_size_ = 0;
  pattern_file_.close();
  uint32_t dt = micros() - t0;
  Serial.printf("[SdManager::closePatternFile]  %lu us\n", dt);
}

PatternHeader SdManager::rewindAndReadHeader() {
  uint32_t t0 = micros();
  if (pattern_cache_ && pattern_cache_size_ >= pattern_header_size) {
    memcpy(&header_, pattern_cache_, pattern_header_size);
  } else {
    pattern_file_.rewind();
    pattern_file_.read(&header_, pattern_header_size);
  }
  uint32_t dt = micros() - t0;
  Serial.printf("[SdManager::rewindAndReadHeader] frames_x=%u frames_y=%u gs=%u rows=%u cols=%u  %lu us\n",
                (unsigned)header_.frame_count_x, (unsigned)header_.frame_count_y,
                (unsigned)header_.grayscale_value,
                (unsigned)header_.panel_count_per_frame_row,
                (unsigned)header_.panel_count_per_frame_col, dt);
  return header_;
}

void SdManager::readFrameFromFile(uint8_t *buffer, uint16_t frame_index,
                                  uint64_t byte_count_per_frame) {
  uint32_t t0 = micros();
  uint32_t file_position = pattern_header_size
                           + frame_index * byte_count_per_frame;

  if (pattern_cache_
      && file_position + (uint32_t)byte_count_per_frame <= pattern_cache_size_) {
    memcpy(buffer, pattern_cache_ + file_position, (uint32_t)byte_count_per_frame);
  } else {
    pattern_file_.seekSet(file_position);
    pattern_file_.read(buffer, byte_count_per_frame);
  }

  uint32_t dt = micros() - t0;
  Serial.printf("[SdManager::readFrameFromFile] frame_index=%u bytes=%lu  %lu us\n",
                frame_index, (uint32_t)byte_count_per_frame, dt);
}

uint64_t SdManager::byteCountPerFrameGrayscale() const {
  uint32_t t0 = micros();
  uint64_t result = (uint64_t)byte_count_per_panel_grayscale
                        * panel_count_per_frame_row * panel_count_per_frame_col
                    + (uint64_t)pattern_row_signifier_byte_count_per_row
                          * panel_count_per_frame_row;
  uint32_t dt = micros() - t0;
  Serial.printf("[SdManager::byteCountPerFrameGrayscale] result=%lu  %lu us\n", (uint32_t)result, dt);
  return result;
}

uint64_t SdManager::byteCountPerFrameBinary() const {
  uint32_t t0 = micros();
  uint64_t result = (uint64_t)byte_count_per_panel_binary
                        * panel_count_per_frame_row * panel_count_per_frame_col
                    + (uint64_t)pattern_row_signifier_byte_count_per_row
                          * panel_count_per_frame_row;
  uint32_t dt = micros() - t0;
  Serial.printf("[SdManager::byteCountPerFrameBinary] result=%lu  %lu us\n", (uint32_t)result, dt);
  return result;
}