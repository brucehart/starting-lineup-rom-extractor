#include "crc32.h"

Crc32::Crc32() {
  reset();
}

void Crc32::reset() {
  state_ = 0xFFFFFFFFUL;
}

void Crc32::update(uint8_t value) {
  state_ ^= value;
  for (uint8_t bit = 0; bit < 8; ++bit) {
    if ((state_ & 1UL) != 0) {
      state_ = (state_ >> 1U) ^ 0xEDB88320UL;
    } else {
      state_ >>= 1U;
    }
  }
}

void Crc32::update(const uint8_t *data, size_t length) {
  if (data == nullptr) {
    return;
  }

  for (size_t index = 0; index < length; ++index) {
    update(data[index]);
  }
}

uint32_t Crc32::finalize() const {
  return ~state_;
}

