# starting_lineup_rom_extractor

Arduino Mega 2560 firmware for reading a Starting Lineup Talking Baseball cartridge ROM and streaming it over USB serial.

## Sketch Layout

Open the sketch in:

`starting_lineup_rom_extractor/starting_lineup_rom_extractor.ino`

Source files:

- `pins.h`
- `bus.h` / `bus.cpp`
- `protocol.h` / `protocol.cpp`
- `crc32.h` / `crc32.cpp`
- `sha1.h` / `sha1.cpp`

## Default Hardware Mapping

- Address `A0..A17`: Mega pins `22..39`
- Data `D0..D7`: Mega pins `42..49`
- `RD_N`: Mega pin `50`
- `CART_EN_N`: Mega pin `51`

Defaults assume:

- ROM size: `131072` bytes
- Baud: `115200`
- `RD_N` inactive high
- `CART_EN_N` inactive high
- Timing profile: `slow-safe`
- Setup delay: `5 us`
- Access delay: `5 us`
- Hold delay: `1 us`

## Serial Commands

- `PING`
- `INFO`
- `READ`
- `READHEX [start length]`
- `READRANGE start length`
- `TEST [addr] [samples]`
- `SCANPOL`
- `VERIFY [passes] [start length]`
- `CRC`
- `SHA1`
- `SETDELAY us`
- `SETTIMING setup access hold`
- `SETINV RD_N|CART_EN_N 0|1`
- `SETSIZE 65536|131072|262144`
- `HELP`

## Bring-Up Flow

1. Wire power, ground, address, data, `RD_N`, and `CART_EN_N`.
2. Upload the sketch to an Arduino Mega 2560.
3. Open Serial Monitor with newline line endings at `115200`.
4. Run `PING`, `INFO`, and `TEST`.
5. Run `SCANPOL` if polarity is uncertain.
6. Adjust timing with `SETDELAY` or `SETTIMING`.
7. Run `VERIFY` or `VERIFY 4` to confirm repeated reads match.
8. Run `READ` to dump the configured ROM size.
9. Run `CRC` and `SHA1` to archive checksums.

## Compile-Time Options

Override these macros if needed before building:

- `STARTING_LINEUP_SERIAL_BAUD`
- `STARTING_LINEUP_FAST_MODE`
- `STARTING_LINEUP_ENABLE_SHA1`

Examples:

- `STARTING_LINEUP_SERIAL_BAUD=230400`
- `STARTING_LINEUP_FAST_MODE=1`
