#pragma once

#include <Arduino.h>

#include "bus.h"

void printStartupBanner(const BusConfig &config, uint32_t baudRate);
void processProtocol(BusConfig &config, uint32_t baudRate);

