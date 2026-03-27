#include "bus.h"

#include "pins.h"

namespace {

bool controlLevelFor(bool active, bool inverted) {
  if (!inverted) {
    return active ? LOW : HIGH;
  }

  return active ? HIGH : LOW;
}

void applyDelay(uint16_t delayUs) {
  if (delayUs > 0) {
    delayMicroseconds(delayUs);
  }
}

bool addressAlreadyListed(uint32_t address, const uint32_t *addresses, uint8_t count) {
  for (uint8_t index = 0; index < count; ++index) {
    if (addresses[index] == address) {
      return true;
    }
  }

  return false;
}

uint8_t buildProbeAddresses(uint32_t romSizeBytes, uint32_t *addresses, uint8_t maxCount) {
  if (maxCount == 0 || romSizeBytes == 0) {
    return 0;
  }

  const uint32_t rawCandidates[] = {
      0UL,
      1UL,
      2UL,
      0x10UL,
      0x55UL,
      0x123UL,
      romSizeBytes / 4UL,
      romSizeBytes / 2UL,
      romSizeBytes - 1UL,
  };

  uint8_t count = 0;
  for (uint8_t index = 0; index < (sizeof(rawCandidates) / sizeof(rawCandidates[0])); ++index) {
    const uint32_t candidate = rawCandidates[index];
    if (candidate >= romSizeBytes) {
      continue;
    }

    if (addressAlreadyListed(candidate, addresses, count)) {
      continue;
    }

    addresses[count++] = candidate;
    if (count >= maxCount) {
      break;
    }
  }

  return count;
}

uint8_t countUniqueValues(const uint8_t *values, uint8_t count) {
  uint8_t uniqueCount = 0;
  for (uint8_t outer = 0; outer < count; ++outer) {
    bool seen = false;
    for (uint8_t inner = 0; inner < outer; ++inner) {
      if (values[inner] == values[outer]) {
        seen = true;
        break;
      }
    }

    if (!seen) {
      ++uniqueCount;
    }
  }

  return uniqueCount;
}

}  // namespace

void initBus(const BusConfig &config) {
  for (uint8_t index = 0; index < kDataBusWidth; ++index) {
    pinMode(kDataPins[index], INPUT);
    digitalWrite(kDataPins[index], LOW);
  }

  for (uint8_t index = 0; index < kAddressBusWidth; ++index) {
    pinMode(kAddressPins[index], OUTPUT);
    digitalWrite(kAddressPins[index], LOW);
  }

  pinMode(kPinRdN, OUTPUT);
  pinMode(kPinCartEnN, OUTPUT);

  setAddress(0UL);
  enableRead(false, config);
  enableCart(false, config);
}

void setAddress(uint32_t addr) {
  for (uint8_t bit = 0; bit < kAddressBusWidth; ++bit) {
    digitalWrite(kAddressPins[bit], (addr & (1UL << bit)) ? HIGH : LOW);
  }
}

void enableCart(bool active, const BusConfig &config) {
  digitalWrite(kPinCartEnN, controlLevelFor(active, config.invertCartEnable));
}

void enableRead(bool active, const BusConfig &config) {
  digitalWrite(kPinRdN, controlLevelFor(active, config.invertRead));
}

uint8_t readDataBus() {
  uint8_t value = 0;
  for (uint8_t bit = 0; bit < kDataBusWidth; ++bit) {
    if (digitalRead(kDataPins[bit]) == HIGH) {
      value |= static_cast<uint8_t>(1U << bit);
    }
  }

  return value;
}

uint8_t readByte(uint32_t addr, const BusConfig &config) {
  setAddress(addr);
  applyDelay(config.timing.setupDelayUs);

  enableCart(true, config);
  enableRead(true, config);
  applyDelay(config.timing.accessDelayUs);

  const uint8_t value = readDataBus();

  enableRead(false, config);
  enableCart(false, config);
  applyDelay(config.timing.holdDelayUs);

  return value;
}

bool isSupportedRomSize(uint32_t sizeBytes) {
  for (uint8_t index = 0; index < (sizeof(kSupportedRomSizes) / sizeof(kSupportedRomSizes[0])); ++index) {
    if (kSupportedRomSizes[index] == sizeBytes) {
      return true;
    }
  }

  return false;
}

bool isValidReadRange(uint32_t start, uint32_t length, const BusConfig &config) {
  if (length == 0 || start >= config.romSizeBytes) {
    return false;
  }

  return length <= (config.romSizeBytes - start);
}

StabilityResult measureAddressStability(uint32_t addr,
                                        uint8_t samples,
                                        const BusConfig &config) {
  StabilityResult result = {
      true,
      0,
      0,
      0,
      samples,
  };

  if (samples == 0) {
    return result;
  }

  result.firstValue = readByte(addr, config);
  for (uint8_t index = 1; index < samples; ++index) {
    const uint8_t value = readByte(addr, config);
    if (value != result.firstValue) {
      result.stable = false;
      result.mismatchValue = value;
      result.mismatchSampleIndex = index;
      break;
    }
  }

  return result;
}

VerificationResult verifyRange(uint32_t start,
                               uint32_t length,
                               uint8_t passes,
                               const BusConfig &config) {
  VerificationResult result = {
      true,
      0,
      0,
      0,
      0,
      passes,
  };

  if (passes < 2 || !isValidReadRange(start, length, config)) {
    result.match = false;
    return result;
  }

  for (uint32_t offset = 0; offset < length; ++offset) {
    const uint32_t address = start + offset;
    const uint8_t expected = readByte(address, config);
    for (uint8_t pass = 1; pass < passes; ++pass) {
      const uint8_t observed = readByte(address, config);
      if (observed != expected) {
        result.match = false;
        result.firstMismatchAddress = address;
        result.expectedValue = expected;
        result.observedValue = observed;
        result.failingPass = static_cast<uint8_t>(pass + 1U);
        return result;
      }
    }
  }

  return result;
}

uint8_t scanControlPolarities(const BusConfig &config,
                              PolarityScanResult *results,
                              uint8_t maxResults) {
  if (results == nullptr || maxResults == 0) {
    return 0;
  }

  uint32_t probeAddresses[8];
  const uint8_t probeCount = buildProbeAddresses(config.romSizeBytes, probeAddresses, 8);
  const uint8_t resultCount = maxResults < 4 ? maxResults : 4;

  for (uint8_t combo = 0; combo < resultCount; ++combo) {
    BusConfig candidate = config;
    candidate.invertRead = (combo & 0x01U) != 0;
    candidate.invertCartEnable = (combo & 0x02U) != 0;

    enableRead(false, candidate);
    enableCart(false, candidate);
    setAddress(0UL);

    PolarityScanResult result = {
        candidate.invertRead,
        candidate.invertCartEnable,
        0,
        probeCount,
        0,
        0,
        false,
        false,
    };

    uint8_t stableValues[8];
    uint8_t stableValueCount = 0;

    for (uint8_t addressIndex = 0; addressIndex < probeCount; ++addressIndex) {
      const StabilityResult stability =
          measureAddressStability(probeAddresses[addressIndex], kDefaultStabilitySamples, candidate);
      if (!stability.stable) {
        continue;
      }

      stableValues[stableValueCount++] = stability.firstValue;
      ++result.stableAddressCount;
    }

    if (stableValueCount > 0) {
      result.firstStableValue = stableValues[0];
      result.uniqueStableValues = countUniqueValues(stableValues, stableValueCount);
      result.likelyFloating =
          (result.uniqueStableValues == 1U) &&
          (result.firstStableValue == 0x00U || result.firstStableValue == 0xFFU);
      result.likelyReadable =
          (result.stableAddressCount == probeCount) &&
          (result.uniqueStableValues > 1U) &&
          !result.likelyFloating;
    }

    results[combo] = result;
  }

  enableRead(false, config);
  enableCart(false, config);
  setAddress(0UL);

  return resultCount;
}

const char *activeLevelLabel(bool inverted) {
  return inverted ? "HIGH" : "LOW";
}

const char *inactiveLevelLabel(bool inverted) {
  return inverted ? "LOW" : "HIGH";
}

