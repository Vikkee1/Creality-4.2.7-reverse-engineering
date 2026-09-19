#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "app.h"
#include "soft_i2c_stm32.h"
#include "eeprom_BL23C16A.h"

static soft_i2c_stm32_ctx_t s_i2c_ctx = {
                        .sda_port = GPIOA,
                        .sda_pin  = GPIO_PIN_11,
                        .scl_port = GPIOA,
                        .scl_pin  = GPIO_PIN_12 };

static soft_i2c_t s_i2c_bus;

/* EEPROMs go busy internally after a write; poll the address with plain
 * probes until it ACKs again. */
static bool eeprom_wait_write_done(uint8_t addr7)
{
    for (uint32_t waited = 0; waited < EEPROM_WRITE_TIMEOUT_MS; waited++) {
        if (soft_i2c_probe(&s_i2c_bus, addr7) == SOFT_I2C_OK) return true;
        delay(1);
    }
    return false;
}

static bool eeprom_write_verify(uint8_t addr7, uint8_t reg,
                                 const uint8_t *pattern, size_t len)
{
    uint8_t readback[EEPROM_PAGE_SIZE];
    soft_i2c_status_t st;

    st = soft_i2c_mem_write(&s_i2c_bus, addr7, reg, pattern, len);
    if (st != SOFT_I2C_OK) {
        uart_logf("  mem_write reg 0x%02X failed: %d\r\n", reg, (int)st);
        return false;
    }

    if (!eeprom_wait_write_done(addr7)) {
        uart_logf("  reg 0x%02X: write cycle timed out\r\n", reg);
        return false;
    }

    st = soft_i2c_mem_read(&s_i2c_bus, addr7, reg, readback, len);
    if (st != SOFT_I2C_OK) {
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
static bool eeprom_self_test(void)
{
    bool ok = true;
    const uint8_t pat_a5[1]   = { 0xA5u };
    const uint8_t pat_5a[1]   = { 0x5Au };
    const uint8_t pat_multi[8] = { 0,1,2,3,4,5,6,7 };

    uart_log("EEPROM self-test (BL23C16A @ soft I2C)\r\n");

    if (soft_i2c_probe(&s_i2c_bus, EEPROM_BASE_ADDR) != SOFT_I2C_OK) {
        uart_log("  probe 0x50: NO ACK - check wiring/pull-ups\r\n");
        return false;
    }
    uart_log("  probe 0x50: ACK\r\n");

    ok &= eeprom_write_verify(EEPROM_BASE_ADDR, 0x00u, pat_a5, sizeof(pat_a5));
    ok &= eeprom_write_verify(EEPROM_BASE_ADDR, 0x10u, pat_5a, sizeof(pat_5a));
    ok &= eeprom_write_verify(EEPROM_BASE_ADDR, 0x20u, pat_multi, sizeof(pat_multi));

    uart_log(ok ? "RESULT: PASS\r\n" : "RESULT: FAIL\r\n");
    return ok;
}

int main_app(void)
{
    soft_i2c_status_t st = soft_i2c_stm32_init(&s_i2c_bus, &s_i2c_ctx, 5u);
    if (st != SOFT_I2C_OK) {
        uart_logf("soft_i2c_stm32_init failed: %d\r\n", (int)st);
        return -1;
    }

    eeprom_self_test();

    return 0;
}
