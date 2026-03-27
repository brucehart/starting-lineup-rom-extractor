#include "bus.h"
#include "pins.h"
#include "protocol.h"

BusConfig gBusConfig = {
    kDefaultRomSizeBytes,
    {
        kDefaultSetupDelayUs,
        kDefaultAccessDelayUs,
        kDefaultHoldDelayUs,
    },
    false,
    false,
    kDefaultVerifyPasses,
};

void setup() {
  initBus(gBusConfig);

  Serial.begin(STARTING_LINEUP_SERIAL_BAUD);
  delay(250);

  printStartupBanner(gBusConfig, STARTING_LINEUP_SERIAL_BAUD);
}

void loop() {
  processProtocol(gBusConfig, STARTING_LINEUP_SERIAL_BAUD);
}
