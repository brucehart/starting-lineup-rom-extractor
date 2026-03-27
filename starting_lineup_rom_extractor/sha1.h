#pragma once

#include <Arduino.h>

class Sha1 {
 public:
  Sha1();

  void reset();
  void update(uint8_t value);
  void update(const uint8_t *data, size_t length);
  void finalize(uint8_t output[20]);

 private:
  void processBlock(const uint8_t block[64]);

  uint32_t state_[5];
  uint64_t totalBytes_;
  uint8_t buffer_[64];
  uint8_t bufferLength_;
  bool finalized_;
};

