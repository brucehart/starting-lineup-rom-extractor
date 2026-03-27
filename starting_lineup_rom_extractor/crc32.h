#pragma once

#include <Arduino.h>

class Crc32 {
 public:
  Crc32();

  void reset();
  void update(uint8_t value);
  void update(const uint8_t *data, size_t length);
  uint32_t finalize() const;

 private:
  uint32_t state_;
};

