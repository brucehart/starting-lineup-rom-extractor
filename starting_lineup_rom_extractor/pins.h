#pragma once

#include <Arduino.h>

constexpr char kFirmwareName[] = "starting_lineup_rom_extractor";
constexpr char kBuildVersion[] = __DATE__ " " __TIME__;

#ifndef STARTING_LINEUP_SERIAL_BAUD
#define STARTING_LINEUP_SERIAL_BAUD 115200UL
#endif

#ifndef STARTING_LINEUP_FAST_MODE
#define STARTING_LINEUP_FAST_MODE 0
#endif

#ifndef STARTING_LINEUP_ENABLE_SHA1
#define STARTING_LINEUP_ENABLE_SHA1 1
#endif

constexpr uint32_t kSupportedBaudRates[] = {
    115200UL,
    230400UL,
};

constexpr uint32_t kSupportedRomSizes[] = {
    65536UL,
    131072UL,
    262144UL,
};

constexpr uint8_t kAddressBusWidth = 18;
constexpr uint8_t kDataBusWidth = 8;

constexpr uint8_t kAddressPins[kAddressBusWidth] = {
    22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39,
};

constexpr uint8_t kDataPins[kDataBusWidth] = {
    42, 43, 44, 45, 46, 47, 48, 49,
};

constexpr uint8_t kPinRdN = 50;
constexpr uint8_t kPinCartEnN = 51;

constexpr uint32_t kDefaultRomSizeBytes = 131072UL;
constexpr uint8_t kDefaultVerifyPasses = 2;
constexpr uint8_t kMaxVerifyPasses = 16;
constexpr uint8_t kDefaultStabilitySamples = 64;

#if STARTING_LINEUP_FAST_MODE
constexpr char kTimingProfileName[] = "fast";
constexpr uint16_t kDefaultSetupDelayUs = 1;
constexpr uint16_t kDefaultAccessDelayUs = 1;
constexpr uint16_t kDefaultHoldDelayUs = 0;
#else
constexpr char kTimingProfileName[] = "slow-safe";
constexpr uint16_t kDefaultSetupDelayUs = 5;
constexpr uint16_t kDefaultAccessDelayUs = 5;
constexpr uint16_t kDefaultHoldDelayUs = 1;
#endif

