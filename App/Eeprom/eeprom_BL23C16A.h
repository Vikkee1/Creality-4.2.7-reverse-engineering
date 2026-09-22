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

/**
 * @brief Wait for the EEPROM's internal write cycle to finish.
 *
 * EEPROMs go busy internally after a write; this polls the device address
 * with plain probes until it ACKs again.
 *
 * @param bus   Bus vtable to issue the probe through.
 * @param addr7 7-bit device address to poll.
 * @return true once the device ACKs again; false if it is still busy
 *         after ::EEPROM_WRITE_TIMEOUT_MS.
 */
bool eeprom_wait_write_done(const i2c_bus_t *bus, uint8_t addr7);

/**
 * @brief Write one full page of data to the EEPROM.
 *
 * The write is not verified or waited on; call eeprom_read_page() (which
 * waits for the write cycle) or eeprom_write_verify() if confirmation is
 * needed.
 *
 * @param bus        Bus vtable used to talk to the device.
 * @param page_index Page index to write, in [0, EEPROM_PAGE_COUNT).
 * @param data       Bytes to write.
 * @param len        Number of bytes in @p data, at most EEPROM_PAGE_SIZE.
 * @return true on success; false on a NULL argument, an out-of-range
 *         @p page_index or @p len, or an I2C failure.
 */
bool eeprom_write_page(const i2c_bus_t *bus, uint8_t page_index, const uint8_t *data, size_t len);

/**
 * @brief Read one full page of data from the EEPROM.
 *
 * Waits for any pending write cycle on the page (via
 * eeprom_wait_write_done()) before reading.
 *
 * @param bus        Bus vtable used to talk to the device.
 * @param page_index Page index to read, in [0, EEPROM_PAGE_COUNT).
 * @param data       Buffer to receive the read bytes.
 * @param len        Number of bytes to read into @p data, at most
 *                    EEPROM_PAGE_SIZE.
 * @return true on success; false on a NULL argument, an out-of-range
 *         @p page_index or @p len, a write-cycle timeout, or an I2C
 *         failure.
 */
bool eeprom_read_page(const i2c_bus_t *bus, uint8_t page_index, uint8_t *data, size_t len);

/**
 * @brief Write a pattern to a register, then read it back and verify it.
 *
 * Writes @p pattern to @p reg, waits for the write cycle to finish, reads
 * it back and compares it against @p pattern. Logs each step via hal.h's
 * uart_log*().
 *
 * @param bus     Bus vtable used to talk to the device.
 * @param addr7   7-bit device address.
 * @param reg     Register/memory address to write and verify.
 * @param pattern Bytes to write and expect back.
 * @param len     Number of bytes in @p pattern, at most EEPROM_PAGE_SIZE.
 * @return true if the write, the write-cycle wait, and the readback
 *         comparison all succeed; false otherwise.
 */
bool eeprom_write_verify(const i2c_bus_t *bus, uint8_t addr7, uint8_t reg,
                          const uint8_t *pattern, size_t len);

/**
 * @brief Bus + EEPROM smoke test.
 *
 * Probes for the device, then round-trips a few independent write/read
 * patterns through the chip via eeprom_write_verify(). Logs
 * "RESULT: PASS" or "RESULT: FAIL".
 *
 * @param bus Bus vtable used to talk to the device.
 * @return true if the device is present and every round trip verifies;
 *         false otherwise.
 */
bool eeprom_self_test(const i2c_bus_t *bus);

#endif
