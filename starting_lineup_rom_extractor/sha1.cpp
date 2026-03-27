#include "sha1.h"

namespace {

uint32_t rotateLeft(uint32_t value, uint8_t bits) {
  return (value << bits) | (value >> (32U - bits));
}

uint32_t readBigEndian32(const uint8_t *buffer) {
  return (static_cast<uint32_t>(buffer[0]) << 24U) |
         (static_cast<uint32_t>(buffer[1]) << 16U) |
         (static_cast<uint32_t>(buffer[2]) << 8U) |
         static_cast<uint32_t>(buffer[3]);
}

}  // namespace

Sha1::Sha1() {
  reset();
}

void Sha1::reset() {
  state_[0] = 0x67452301UL;
  state_[1] = 0xEFCDAB89UL;
  state_[2] = 0x98BADCFEUL;
  state_[3] = 0x10325476UL;
  state_[4] = 0xC3D2E1F0UL;
  totalBytes_ = 0;
  bufferLength_ = 0;
  finalized_ = false;
}

void Sha1::update(uint8_t value) {
  update(&value, 1U);
}

void Sha1::update(const uint8_t *data, size_t length) {
  if (data == nullptr || finalized_) {
    return;
  }

  for (size_t index = 0; index < length; ++index) {
    buffer_[bufferLength_++] = data[index];
    ++totalBytes_;

    if (bufferLength_ == sizeof(buffer_)) {
      processBlock(buffer_);
      bufferLength_ = 0;
    }
  }
}

void Sha1::finalize(uint8_t output[20]) {
  if (output == nullptr) {
    return;
  }

  if (finalized_) {
    for (uint8_t index = 0; index < 5; ++index) {
      output[(index * 4U) + 0U] = static_cast<uint8_t>(state_[index] >> 24U);
      output[(index * 4U) + 1U] = static_cast<uint8_t>(state_[index] >> 16U);
      output[(index * 4U) + 2U] = static_cast<uint8_t>(state_[index] >> 8U);
      output[(index * 4U) + 3U] = static_cast<uint8_t>(state_[index]);
    }
    return;
  }

  const uint64_t totalBits = totalBytes_ * 8ULL;
  buffer_[bufferLength_++] = 0x80U;

  if (bufferLength_ > 56U) {
    while (bufferLength_ < 64U) {
      buffer_[bufferLength_++] = 0x00U;
    }
    processBlock(buffer_);
    bufferLength_ = 0;
  }

  while (bufferLength_ < 56U) {
    buffer_[bufferLength_++] = 0x00U;
  }

  for (int8_t shift = 56; shift >= 0; shift -= 8) {
    buffer_[bufferLength_++] = static_cast<uint8_t>(totalBits >> shift);
  }

  processBlock(buffer_);
  bufferLength_ = 0;
  finalized_ = true;

  for (uint8_t index = 0; index < 5; ++index) {
    output[(index * 4U) + 0U] = static_cast<uint8_t>(state_[index] >> 24U);
    output[(index * 4U) + 1U] = static_cast<uint8_t>(state_[index] >> 16U);
    output[(index * 4U) + 2U] = static_cast<uint8_t>(state_[index] >> 8U);
    output[(index * 4U) + 3U] = static_cast<uint8_t>(state_[index]);
  }
}

void Sha1::processBlock(const uint8_t block[64]) {
  uint32_t words[80];

  for (uint8_t index = 0; index < 16; ++index) {
    words[index] = readBigEndian32(&block[index * 4U]);
  }

  for (uint8_t index = 16; index < 80; ++index) {
    words[index] = rotateLeft(words[index - 3U] ^
                              words[index - 8U] ^
                              words[index - 14U] ^
                              words[index - 16U],
                              1U);
  }

  uint32_t a = state_[0];
  uint32_t b = state_[1];
  uint32_t c = state_[2];
  uint32_t d = state_[3];
  uint32_t e = state_[4];

  for (uint8_t index = 0; index < 80; ++index) {
    uint32_t f = 0;
    uint32_t k = 0;

    if (index < 20U) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999UL;
    } else if (index < 40U) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1UL;
    } else if (index < 60U) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCUL;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6UL;
    }

    const uint32_t temp = rotateLeft(a, 5U) + f + e + k + words[index];
    e = d;
    d = c;
    c = rotateLeft(b, 30U);
    b = a;
    a = temp;
  }

  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
}

