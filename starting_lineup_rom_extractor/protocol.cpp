#include "protocol.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bus.h"
#include "crc32.h"
#include "pins.h"
#if STARTING_LINEUP_ENABLE_SHA1
#include "sha1.h"
#endif

namespace {

constexpr size_t kCommandBufferSize = 96;
char gCommandBuffer[kCommandBufferSize];
size_t gCommandLength = 0;
bool gDiscardUntilNewline = false;

bool equalsIgnoreCase(const char *left, const char *right) {
  if (left == nullptr || right == nullptr) {
    return false;
  }

  while (*left != '\0' && *right != '\0') {
    if (toupper(static_cast<unsigned char>(*left)) !=
        toupper(static_cast<unsigned char>(*right))) {
      return false;
    }
    ++left;
    ++right;
  }

  return *left == '\0' && *right == '\0';
}

char *trimWhitespace(char *text) {
  if (text == nullptr) {
    return text;
  }

  while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
    ++text;
  }

  char *end = text + strlen(text);
  while (end > text && isspace(static_cast<unsigned char>(*(end - 1)))) {
    --end;
  }

  *end = '\0';
  return text;
}

bool parseUnsignedArg(const char *token, uint32_t *value) {
  if (token == nullptr || value == nullptr) {
    return false;
  }

  char *end = nullptr;
  const unsigned long parsed = strtoul(token, &end, 0);
  if (end == token || *end != '\0') {
    return false;
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

void printHexFixed(uint32_t value, uint8_t width) {
  for (int shift = (static_cast<int>(width) - 1) * 4; shift >= 0; shift -= 4) {
    const uint8_t digit = static_cast<uint8_t>((value >> shift) & 0x0FU);
    Serial.write(digit < 10U ? ('0' + digit) : ('A' + digit - 10U));
  }
}

void printHexByte(uint8_t value) {
  printHexFixed(value, 2U);
}

void printHexAddress(uint32_t value) {
  printHexFixed(value, 5U);
}

void printPinList(const __FlashStringHelper *label, const uint8_t *pins, uint8_t count) {
  Serial.print(label);
  for (uint8_t index = 0; index < count; ++index) {
    if (index > 0) {
      Serial.write(',');
    }
    Serial.print(pins[index]);
  }
  Serial.println();
}

void printControlSummaryLine(const __FlashStringHelper *name, bool inverted) {
  Serial.print(name);
  Serial.print(F(": active="));
  Serial.print(activeLevelLabel(inverted));
  Serial.print(F(" inactive="));
  Serial.print(inactiveLevelLabel(inverted));
  Serial.print(F(" inverted="));
  Serial.println(inverted ? 1 : 0);
}

void printPinMapping() {
  printPinList(F("PINMAP ADDR[0..17]="), kAddressPins, kAddressBusWidth);
  printPinList(F("PINMAP DATA[0..7]="), kDataPins, kDataBusWidth);
  Serial.print(F("PINMAP RD_N="));
  Serial.print(kPinRdN);
  Serial.print(F(" CART_EN_N="));
  Serial.println(kPinCartEnN);
}

void printHelp() {
  Serial.println(F("COMMANDS"));
  Serial.println(F("  PING"));
  Serial.println(F("  INFO"));
  Serial.println(F("  READ"));
  Serial.println(F("  READHEX [start length]"));
  Serial.println(F("  READRANGE start length"));
  Serial.println(F("  TEST [addr] [samples]"));
  Serial.println(F("  SCANPOL"));
  Serial.println(F("  VERIFY [passes] [start length]"));
  Serial.println(F("  CRC"));
  Serial.println(F("  SHA1"));
  Serial.println(F("  SETDELAY us"));
  Serial.println(F("  SETTIMING setup access hold"));
  Serial.println(F("  SETINV RD_N|CART_EN_N 0|1"));
  Serial.println(F("  SETSIZE 65536|131072|262144"));
  Serial.println(F("  HELP"));
}

void printInfo(const BusConfig &config, uint32_t baudRate) {
  Serial.print(F("NAME "));
  Serial.println(kFirmwareName);
  Serial.print(F("VERSION "));
  Serial.println(kBuildVersion);
  Serial.print(F("BAUD "));
  Serial.println(baudRate);
  Serial.print(F("TIMING_PROFILE "));
  Serial.println(kTimingProfileName);
  Serial.print(F("ROM_SIZE "));
  Serial.println(config.romSizeBytes);
  Serial.print(F("VERIFY_PASSES "));
  Serial.println(config.verifyPasses);
  Serial.print(F("TIMING setup_us="));
  Serial.print(config.timing.setupDelayUs);
  Serial.print(F(" access_us="));
  Serial.print(config.timing.accessDelayUs);
  Serial.print(F(" hold_us="));
  Serial.println(config.timing.holdDelayUs);
  printControlSummaryLine(F("POLARITY RD_N"), config.invertRead);
  printControlSummaryLine(F("POLARITY CART_EN_N"), config.invertCartEnable);
  printPinMapping();
  Serial.print(F("SUPPORTED_BAUD "));
  Serial.print(kSupportedBaudRates[0]);
  Serial.print(F(","));
  Serial.println(kSupportedBaudRates[1]);
  Serial.println(F("SUPPORTED_ROM_SIZES 65536,131072,262144"));
}

void printReadHeader(const __FlashStringHelper *label, uint32_t start, uint32_t length) {
  Serial.print(F("BEGIN "));
  Serial.print(label);
  Serial.print(F(" start=0x"));
  printHexAddress(start);
  Serial.print(F(" length="));
  Serial.println(length);
}

void printReadFooter(const __FlashStringHelper *label, uint32_t length, uint32_t crc32) {
  Serial.print(F("END "));
  Serial.print(label);
  Serial.print(F(" bytes="));
  Serial.print(length);
  Serial.print(F(" crc32=0x"));
  printHexFixed(crc32, 8U);
  Serial.println();
}

void streamBinaryRange(const __FlashStringHelper *label,
                       uint32_t start,
                       uint32_t length,
                       const BusConfig &config) {
  Crc32 crc;
  printReadHeader(label, start, length);
  for (uint32_t offset = 0; offset < length; ++offset) {
    const uint8_t value = readByte(start + offset, config);
    crc.update(value);
    Serial.write(value);
  }
  Serial.println();
  printReadFooter(label, length, crc.finalize());
}

void streamHexRange(uint32_t start, uint32_t length, const BusConfig &config) {
  Crc32 crc;
  printReadHeader(F("READHEX"), start, length);
  for (uint32_t lineStart = 0; lineStart < length; lineStart += 16UL) {
    const uint32_t address = start + lineStart;
    printHexAddress(address);
    Serial.print(F(": "));

    const uint32_t lineLength = ((length - lineStart) < 16UL) ? (length - lineStart) : 16UL;
    for (uint32_t index = 0; index < lineLength; ++index) {
      const uint8_t value = readByte(address + index, config);
      crc.update(value);
      printHexByte(value);
      if (index + 1UL < lineLength) {
        Serial.write(' ');
      }
    }
    Serial.println();
  }
  printReadFooter(F("READHEX"), length, crc.finalize());
}

void printStabilityLine(uint32_t address, const StabilityResult &result) {
  Serial.print(F("ADDR 0x"));
  printHexAddress(address);
  Serial.print(F(" stable="));
  Serial.print(result.stable ? 1 : 0);
  Serial.print(F(" value=0x"));
  printHexByte(result.firstValue);
  if (!result.stable) {
    Serial.print(F(" mismatch=0x"));
    printHexByte(result.mismatchValue);
    Serial.print(F(" sample="));
    Serial.print(result.mismatchSampleIndex);
  }
  Serial.println();
}

void runDefaultTest(const BusConfig &config) {
  const uint32_t candidateAddresses[] = {
      0UL,
      1UL,
      0x10UL,
      0x55UL,
      config.romSizeBytes / 2UL,
      config.romSizeBytes - 1UL,
  };

  uint8_t stableCount = 0;
  Serial.print(F("TEST samples="));
  Serial.println(kDefaultStabilitySamples);
  for (uint8_t index = 0; index < (sizeof(candidateAddresses) / sizeof(candidateAddresses[0])); ++index) {
    const uint32_t address = candidateAddresses[index];
    if (address >= config.romSizeBytes) {
      continue;
    }
    const StabilityResult result =
        measureAddressStability(address, kDefaultStabilitySamples, config);
    if (result.stable) {
      ++stableCount;
    }
    printStabilityLine(address, result);
  }

  Serial.print(F("TEST_RESULT stable_addresses="));
  Serial.print(stableCount);
  Serial.println(F(" note=run SCANPOL to compare all polarity combinations"));
}

void runAddressTest(uint32_t address, uint8_t samples, const BusConfig &config) {
  const StabilityResult result = measureAddressStability(address, samples, config);
  Serial.print(F("TEST addr=0x"));
  printHexAddress(address);
  Serial.print(F(" samples="));
  Serial.println(samples);
  printStabilityLine(address, result);
}

void runPolarityScan(const BusConfig &config) {
  PolarityScanResult results[4];
  const uint8_t resultCount = scanControlPolarities(config, results, 4U);

  Serial.println(F("SCANPOL"));
  for (uint8_t index = 0; index < resultCount; ++index) {
    const PolarityScanResult &result = results[index];
    Serial.print(F("RD_N_INV="));
    Serial.print(result.invertRead ? 1 : 0);
    Serial.print(F(" CART_EN_N_INV="));
    Serial.print(result.invertCartEnable ? 1 : 0);
    Serial.print(F(" stable_addrs="));
    Serial.print(result.stableAddressCount);
    Serial.print(F("/"));
    Serial.print(result.probedAddressCount);
    Serial.print(F(" unique_values="));
    Serial.print(result.uniqueStableValues);
    Serial.print(F(" first=0x"));
    printHexByte(result.firstStableValue);
    Serial.print(F(" floating="));
    Serial.print(result.likelyFloating ? 1 : 0);
    Serial.print(F(" likely_readable="));
    Serial.println(result.likelyReadable ? 1 : 0);
  }
}

void printVerificationResult(const VerificationResult &result,
                             uint32_t start,
                             uint32_t length) {
  if (result.match) {
    Serial.print(F("VERIFY OK start=0x"));
    printHexAddress(start);
    Serial.print(F(" length="));
    Serial.print(length);
    Serial.print(F(" passes="));
    Serial.println(result.passesChecked);
    return;
  }

  Serial.print(F("VERIFY FAIL addr=0x"));
  printHexAddress(result.firstMismatchAddress);
  Serial.print(F(" expected=0x"));
  printHexByte(result.expectedValue);
  Serial.print(F(" observed=0x"));
  printHexByte(result.observedValue);
  Serial.print(F(" pass="));
  Serial.println(result.failingPass);
}

void printCrc32(const BusConfig &config) {
  Crc32 crc;
  for (uint32_t offset = 0; offset < config.romSizeBytes; ++offset) {
    crc.update(readByte(offset, config));
  }

  Serial.print(F("CRC32 bytes="));
  Serial.print(config.romSizeBytes);
  Serial.print(F(" value=0x"));
  printHexFixed(crc.finalize(), 8U);
  Serial.println();
}

#if STARTING_LINEUP_ENABLE_SHA1
void printSha1(const BusConfig &config) {
  Sha1 sha1;
  for (uint32_t offset = 0; offset < config.romSizeBytes; ++offset) {
    sha1.update(readByte(offset, config));
  }

  uint8_t digest[20];
  sha1.finalize(digest);

  Serial.print(F("SHA1 bytes="));
  Serial.print(config.romSizeBytes);
  Serial.print(F(" value="));
  for (uint8_t index = 0; index < sizeof(digest); ++index) {
    printHexByte(digest[index]);
  }
  Serial.println();
}
#endif

void printError(const __FlashStringHelper *message) {
  Serial.print(F("ERR "));
  Serial.println(message);
}

void printOk(const __FlashStringHelper *message) {
  Serial.print(F("OK "));
  Serial.println(message);
}

bool parseRangeArgs(char *startToken,
                    char *lengthToken,
                    uint32_t *start,
                    uint32_t *length,
                    const BusConfig &config) {
  if (!parseUnsignedArg(startToken, start) || !parseUnsignedArg(lengthToken, length)) {
    printError(F("invalid numeric range"));
    return false;
  }

  if (!isValidReadRange(*start, *length, config)) {
    printError(F("range outside configured ROM size"));
    return false;
  }

  return true;
}

void applyInactiveControlLevels(const BusConfig &config) {
  enableRead(false, config);
  enableCart(false, config);
}

void handleCommand(char *line, BusConfig &config, uint32_t baudRate) {
  char *trimmed = trimWhitespace(line);
  if (trimmed == nullptr || *trimmed == '\0') {
    return;
  }

  char *savePtr = nullptr;
  char *command = strtok_r(trimmed, " \t", &savePtr);
  char *arg1 = strtok_r(nullptr, " \t", &savePtr);
  char *arg2 = strtok_r(nullptr, " \t", &savePtr);
  char *arg3 = strtok_r(nullptr, " \t", &savePtr);
  char *arg4 = strtok_r(nullptr, " \t", &savePtr);

  if (arg4 != nullptr) {
    printError(F("too many arguments"));
    return;
  }

  if (equalsIgnoreCase(command, "PING")) {
    Serial.println(F("PONG"));
    return;
  }

  if (equalsIgnoreCase(command, "INFO")) {
    printInfo(config, baudRate);
    return;
  }

  if (equalsIgnoreCase(command, "HELP")) {
    printHelp();
    return;
  }

  if (equalsIgnoreCase(command, "READ")) {
    if (arg1 != nullptr) {
      printError(F("READ takes no arguments"));
      return;
    }
    streamBinaryRange(F("READ"), 0UL, config.romSizeBytes, config);
    return;
  }

  if (equalsIgnoreCase(command, "READHEX")) {
    uint32_t start = 0UL;
    uint32_t length = config.romSizeBytes;
    if (arg1 != nullptr || arg2 != nullptr) {
      if (arg1 == nullptr || arg2 == nullptr || arg3 != nullptr) {
        printError(F("READHEX expects zero args or start length"));
        return;
      }
      if (!parseRangeArgs(arg1, arg2, &start, &length, config)) {
        return;
      }
    }
    streamHexRange(start, length, config);
    return;
  }

  if (equalsIgnoreCase(command, "READRANGE")) {
    uint32_t start = 0;
    uint32_t length = 0;
    if (arg1 == nullptr || arg2 == nullptr || arg3 != nullptr) {
      printError(F("READRANGE expects start length"));
      return;
    }
    if (!parseRangeArgs(arg1, arg2, &start, &length, config)) {
      return;
    }
    streamBinaryRange(F("READRANGE"), start, length, config);
    return;
  }

  if (equalsIgnoreCase(command, "TEST")) {
    if (arg1 == nullptr) {
      runDefaultTest(config);
      return;
    }

    uint32_t address = 0;
    uint32_t samples = kDefaultStabilitySamples;
    if (!parseUnsignedArg(arg1, &address)) {
      printError(F("invalid test address"));
      return;
    }
    if (arg2 != nullptr && !parseUnsignedArg(arg2, &samples)) {
      printError(F("invalid sample count"));
      return;
    }
    if (arg3 != nullptr) {
      printError(F("TEST expects addr [samples]"));
      return;
    }
    if (address >= config.romSizeBytes) {
      printError(F("test address outside ROM"));
      return;
    }
    if (samples == 0 || samples > 255UL) {
      printError(F("sample count must be 1..255"));
      return;
    }
    runAddressTest(address, static_cast<uint8_t>(samples), config);
    return;
  }

  if (equalsIgnoreCase(command, "SCANPOL")) {
    if (arg1 != nullptr) {
      printError(F("SCANPOL takes no arguments"));
      return;
    }
    runPolarityScan(config);
    return;
  }

  if (equalsIgnoreCase(command, "VERIFY")) {
    uint32_t passes = config.verifyPasses;
    uint32_t start = 0UL;
    uint32_t length = config.romSizeBytes;

    if (arg1 == nullptr) {
      // Use default full-ROM verification.
    } else if (arg2 == nullptr && arg3 == nullptr) {
      if (!parseUnsignedArg(arg1, &passes)) {
        printError(F("invalid pass count"));
        return;
      }
    } else if (arg1 != nullptr && arg2 != nullptr && arg3 == nullptr) {
      if (!parseRangeArgs(arg1, arg2, &start, &length, config)) {
        return;
      }
    } else if (arg1 != nullptr && arg2 != nullptr && arg3 != nullptr) {
      if (!parseUnsignedArg(arg1, &passes)) {
        printError(F("invalid pass count"));
        return;
      }
      if (!parseRangeArgs(arg2, arg3, &start, &length, config)) {
        return;
      }
    } else {
      printError(F("VERIFY syntax is VERIFY [passes] [start length]"));
      return;
    }

    if (passes < 2UL || passes > kMaxVerifyPasses) {
      printError(F("passes must be 2..16"));
      return;
    }

    const VerificationResult result =
        verifyRange(start, length, static_cast<uint8_t>(passes), config);
    printVerificationResult(result, start, length);
    return;
  }

  if (equalsIgnoreCase(command, "CRC")) {
    if (arg1 != nullptr) {
      printError(F("CRC takes no arguments"));
      return;
    }
    printCrc32(config);
    return;
  }

  if (equalsIgnoreCase(command, "SHA1")) {
    if (arg1 != nullptr) {
      printError(F("SHA1 takes no arguments"));
      return;
    }
#if STARTING_LINEUP_ENABLE_SHA1
    printSha1(config);
#else
    Serial.println(F("SHA1 omitted at compile time"));
#endif
    return;
  }

  if (equalsIgnoreCase(command, "SETDELAY")) {
    uint32_t delayUs = 0;
    if (arg1 == nullptr || arg2 != nullptr || !parseUnsignedArg(arg1, &delayUs)) {
      printError(F("SETDELAY expects one numeric argument"));
      return;
    }
    if (delayUs > 65535UL) {
      printError(F("delay too large"));
      return;
    }
    config.timing.setupDelayUs = static_cast<uint16_t>(delayUs);
    config.timing.accessDelayUs = static_cast<uint16_t>(delayUs);
    applyInactiveControlLevels(config);
    Serial.print(F("OK delay setup_us="));
    Serial.print(config.timing.setupDelayUs);
    Serial.print(F(" access_us="));
    Serial.print(config.timing.accessDelayUs);
    Serial.print(F(" hold_us="));
    Serial.println(config.timing.holdDelayUs);
    return;
  }

  if (equalsIgnoreCase(command, "SETTIMING")) {
    uint32_t setupUs = 0;
    uint32_t accessUs = 0;
    uint32_t holdUs = 0;
    if (arg1 == nullptr || arg2 == nullptr || arg3 == nullptr ||
        !parseUnsignedArg(arg1, &setupUs) ||
        !parseUnsignedArg(arg2, &accessUs) ||
        !parseUnsignedArg(arg3, &holdUs)) {
      printError(F("SETTIMING expects setup access hold"));
      return;
    }
    if (setupUs > 65535UL || accessUs > 65535UL || holdUs > 65535UL) {
      printError(F("timing value too large"));
      return;
    }
    config.timing.setupDelayUs = static_cast<uint16_t>(setupUs);
    config.timing.accessDelayUs = static_cast<uint16_t>(accessUs);
    config.timing.holdDelayUs = static_cast<uint16_t>(holdUs);
    applyInactiveControlLevels(config);
    Serial.print(F("OK timing setup_us="));
    Serial.print(config.timing.setupDelayUs);
    Serial.print(F(" access_us="));
    Serial.print(config.timing.accessDelayUs);
    Serial.print(F(" hold_us="));
    Serial.println(config.timing.holdDelayUs);
    return;
  }

  if (equalsIgnoreCase(command, "SETINV")) {
    uint32_t inverted = 0;
    if (arg1 == nullptr || arg2 == nullptr || arg3 != nullptr || !parseUnsignedArg(arg2, &inverted) ||
        inverted > 1UL) {
      printError(F("SETINV expects name and 0|1"));
      return;
    }

    if (equalsIgnoreCase(arg1, "RD_N") || equalsIgnoreCase(arg1, "RD")) {
      config.invertRead = (inverted != 0UL);
      applyInactiveControlLevels(config);
      printOk(F("RD_N polarity updated"));
      return;
    }

    if (equalsIgnoreCase(arg1, "CART_EN_N") ||
        equalsIgnoreCase(arg1, "CART_EN") ||
        equalsIgnoreCase(arg1, "CART")) {
      config.invertCartEnable = (inverted != 0UL);
      applyInactiveControlLevels(config);
      printOk(F("CART_EN_N polarity updated"));
      return;
    }

    printError(F("unknown invert target"));
    return;
  }

  if (equalsIgnoreCase(command, "SETSIZE")) {
    uint32_t sizeBytes = 0;
    if (arg1 == nullptr || arg2 != nullptr || !parseUnsignedArg(arg1, &sizeBytes)) {
      printError(F("SETSIZE expects one numeric argument"));
      return;
    }
    if (!isSupportedRomSize(sizeBytes)) {
      printError(F("supported sizes are 65536, 131072, 262144"));
      return;
    }
    config.romSizeBytes = sizeBytes;
    Serial.print(F("OK rom_size="));
    Serial.println(config.romSizeBytes);
    return;
  }

  printError(F("unknown command"));
}

}  // namespace

void printStartupBanner(const BusConfig &config, uint32_t baudRate) {
  Serial.println();
  Serial.println(kFirmwareName);
  Serial.print(F("baud="));
  Serial.println(baudRate);
  printControlSummaryLine(F("RD_N"), config.invertRead);
  printControlSummaryLine(F("CART_EN_N"), config.invertCartEnable);
  printPinMapping();
  printHelp();
}

void processProtocol(BusConfig &config, uint32_t baudRate) {
  while (Serial.available() > 0) {
    const int incoming = Serial.read();
    if (incoming < 0) {
      return;
    }

    const char ch = static_cast<char>(incoming);
    if (gDiscardUntilNewline) {
      if (ch == '\n') {
        gDiscardUntilNewline = false;
      }
      continue;
    }

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      gCommandBuffer[gCommandLength] = '\0';
      handleCommand(gCommandBuffer, config, baudRate);
      gCommandLength = 0;
      continue;
    }

    if (gCommandLength >= (kCommandBufferSize - 1U)) {
      gCommandLength = 0;
      gDiscardUntilNewline = true;
      printError(F("command too long"));
      continue;
    }

    gCommandBuffer[gCommandLength++] = ch;
  }
}
