#pragma once

#include <Arduino.h>

struct TimingConfig {
  uint16_t setupDelayUs;
  uint16_t accessDelayUs;
  uint16_t holdDelayUs;
};

struct BusConfig {
  uint32_t romSizeBytes;
  TimingConfig timing;
  bool invertRead;
  bool invertCartEnable;
  uint8_t verifyPasses;
};

struct StabilityResult {
  bool stable;
  uint8_t firstValue;
  uint8_t mismatchValue;
  uint8_t mismatchSampleIndex;
  uint8_t samplesTaken;
};

struct VerificationResult {
  bool match;
  uint32_t firstMismatchAddress;
  uint8_t expectedValue;
  uint8_t observedValue;
  uint8_t failingPass;
  uint8_t passesChecked;
};

struct PolarityScanResult {
  bool invertRead;
  bool invertCartEnable;
  uint8_t stableAddressCount;
  uint8_t probedAddressCount;
  uint8_t uniqueStableValues;
  uint8_t firstStableValue;
  bool likelyFloating;
  bool likelyReadable;
};

void initBus(const BusConfig &config);
void setAddress(uint32_t addr);
void enableCart(bool active, const BusConfig &config);
void enableRead(bool active, const BusConfig &config);
uint8_t readDataBus();
uint8_t readByte(uint32_t addr, const BusConfig &config);

bool isSupportedRomSize(uint32_t sizeBytes);
bool isValidReadRange(uint32_t start, uint32_t length, const BusConfig &config);

StabilityResult measureAddressStability(uint32_t addr,
                                        uint8_t samples,
                                        const BusConfig &config);

VerificationResult verifyRange(uint32_t start,
                               uint32_t length,
                               uint8_t passes,
                               const BusConfig &config);

uint8_t scanControlPolarities(const BusConfig &config,
                              PolarityScanResult *results,
                              uint8_t maxResults);

const char *activeLevelLabel(bool inverted);
const char *inactiveLevelLabel(bool inverted);

