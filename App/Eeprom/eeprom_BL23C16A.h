#ifndef EEPROM_BL23C16A_H
#define EEPROM_BL23C16A_H

#include "i2c_bus.h"
#include "hal.h"
#include <stdint.h>

/* BL23C16A / 24C16: 2 KB EEPROM */
#define EEPROM_PAGE_COUNT       128u
#define EEPROM_BASE_ADDR        0x50u
#define EEPROM_PAGE_SIZE        16u
#define EEPROM_WRITE_TIMEOUT_MS 20u

/* EEPROMs go busy internally after a write; poll the address with plain
 * probes until it ACKs again. */
bool eeprom_wait_write_done(const i2c_bus_t *bus, uint8_t addr7);

bool eeprom_full_erase(const i2c_bus_t *bus);

/* Writes `pattern` to reg, waits for the write cycle, reads it back and
 * verifies it matches. Logs each step via hal.h's uart_log*(). */
bool eeprom_write_verify(const i2c_bus_t *bus, uint8_t addr7, uint8_t reg,
                          const uint8_t *pattern, size_t len);

/* Bus + EEPROM smoke test: presence, then a few independent write/read
 * round trips through the chip. Logs RESULT: PASS/FAIL. */
bool eeprom_self_test(const i2c_bus_t *bus);

#endif
