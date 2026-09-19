#include "eeprom_BL23C16A.h"
#include <string.h>

bool eeprom_wait_write_done(const i2c_bus_t *bus, uint8_t addr7)
{
    for (uint32_t waited = 0; waited < EEPROM_WRITE_TIMEOUT_MS; waited++) {
        if (bus->probe(bus->ctx, addr7) == I2C_BUS_OK) return true;
        delay(1);
    }
    return false;
}

bool eeprom_full_erase(const i2c_bus_t *bus){

}

bool eeprom_write_verify(const i2c_bus_t *bus, uint8_t addr7, uint8_t reg,
                          const uint8_t *pattern, size_t len)
{
    uint8_t readback[EEPROM_PAGE_SIZE];
    i2c_status_t st;

    st = bus->mem_write(bus->ctx, addr7, reg, pattern, len);
    if (st != I2C_BUS_OK) {
        uart_logf("  mem_write reg 0x%02X failed: %d\r\n", reg, (int)st);
        return false;
    }

    if (!eeprom_wait_write_done(bus, addr7)) {
        uart_logf("  reg 0x%02X: write cycle timed out\r\n", reg);
        return false;
    }

    st = bus->mem_read(bus->ctx, addr7, reg, readback, len);
    if (st != I2C_BUS_OK) {
        uart_logf("  mem_read reg 0x%02X failed: %d\r\n", reg, (int)st);
        return false;
    }else {
        for(uint8_t i = 0; i < EEPROM_PAGE_SIZE; i++){
            uart_logf("%d ", readback[i]);
        }
        uart_log("\r\n");
    }

    if (memcmp(pattern, readback, len) != 0) {
        uart_logf("  reg 0x%02X: readback mismatch\r\n", reg);
        return false;
    }

    uart_logf("  reg 0x%02X: write/read OK (%u bytes)\r\n", reg, (unsigned)len);
    return true;
}

/* Bus + EEPROM smoke test: presence, then two independent byte patterns,
 * then a short multi-byte pattern, all round-tripped through the chip. */
bool eeprom_self_test(const i2c_bus_t *bus)
{
    bool ok = true;
    const uint8_t pat_a5[1]   = { 0xA5u };
    const uint8_t pat_5a[1]   = { 0x5Au };
    const uint8_t pat_multi[8] = { 0,1,2,3,4,5,6,7 };

    uart_log("EEPROM self-test (BL23C16A)\r\n");

    if (bus->probe(bus->ctx, EEPROM_BASE_ADDR) != I2C_BUS_OK) {
        uart_log("  probe 0x50: NO ACK - check wiring/pull-ups\r\n");
        return false;
    }
    uart_log("  probe 0x50: ACK\r\n");

    ok &= eeprom_write_verify(bus, EEPROM_BASE_ADDR, 0x00u, pat_a5, sizeof(pat_a5));
    ok &= eeprom_write_verify(bus, EEPROM_BASE_ADDR, 0x10u, pat_5a, sizeof(pat_5a));
    ok &= eeprom_write_verify(bus, EEPROM_BASE_ADDR, 0x20u, pat_multi, sizeof(pat_multi));

    uart_log(ok ? "RESULT: PASS\r\n" : "RESULT: FAIL\r\n");
    return ok;
}
