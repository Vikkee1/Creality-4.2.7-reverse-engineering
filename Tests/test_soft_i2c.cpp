#include "mock_bus.h"
#include <gtest/gtest.h>
#include <cstring>

namespace {

constexpr uint8_t SLAVE_ADDR = 0x50;

void setup(fake_bus_t *fb, soft_i2c_t *bus)
{
    soft_i2c_port_t port;
    fake_bus_init(fb);
    fake_bus_attach_slave(fb, SLAVE_ADDR);
    port = fake_bus_port(fb);
    soft_i2c_init(bus, &port, 5, 2000);
}

} // namespace

/* ---------------------------------------------------------------- */

TEST(SoftI2C, ProbeFindsPresentDevice)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_probe(&bus, SLAVE_ADDR), SOFT_I2C_OK)
        << "present device should ACK";
    EXPECT_TRUE(fake_bus_is_idle(&fb))
        << "bus should return to idle after STOP";
}

TEST(SoftI2C, ProbeMissingDeviceNacks)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_probe(&bus, 0x21), SOFT_I2C_ERR_NACK_ADDR)
        << "absent device must report NACK_ADDR";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "bus idle after failed probe";
}

TEST(SoftI2C, ScanFindsExactlyOne)
{
    fake_bus_t fb; soft_i2c_t bus;
    int found = 0, found_addr = -1;
    setup(&fb, &bus);

    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (soft_i2c_probe(&bus, a) == SOFT_I2C_OK) { found++; found_addr = a; }
    }
    EXPECT_EQ(found, 1) << "scan should find exactly one device";
    EXPECT_EQ(found_addr, SLAVE_ADDR) << "scan should find it at the right address";
}

TEST(SoftI2C, MemWriteLandsInSlaveMemory)
{
    fake_bus_t fb; soft_i2c_t bus;
    const uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_mem_write(&bus, SLAVE_ADDR, 0x10, payload, 4), SOFT_I2C_OK)
        << "mem_write should succeed";
    EXPECT_EQ(fb.mem[0x10], 0xDE) << "byte 0 stored";
    EXPECT_EQ(fb.mem[0x11], 0xAD) << "byte 1 stored";
    EXPECT_EQ(fb.mem[0x12], 0xBE) << "byte 2 stored";
    EXPECT_EQ(fb.mem[0x13], 0xEF) << "byte 3 stored";
}

TEST(SoftI2C, MemReadReturnsSlaveMemory)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[4] = {0};
    setup(&fb, &bus);

    fb.mem[0x20] = 0x01; fb.mem[0x21] = 0x23;
    fb.mem[0x22] = 0x45; fb.mem[0x23] = 0x67;

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x20, buf, 4), SOFT_I2C_OK)
        << "mem_read should succeed";
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x23);
    EXPECT_EQ(buf[2], 0x45);
    EXPECT_EQ(buf[3], 0x67);
}

TEST(SoftI2C, RoundtripAllByteValues)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    /* Catches shift/mask bugs that a single 0xAA test would miss. Fatal
     * ASSERT so a mismatch stops the sweep immediately, same as the
     * original hand-rolled loop's `break`. */
    for (int v = 0; v < 256; v++) {
        uint8_t w = (uint8_t)v, r = 0;
        soft_i2c_mem_write(&bus, SLAVE_ADDR, 0x00, &w, 1);
        soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x00, &r, 1);
        ASSERT_EQ(r, w) << "roundtrip mismatch at value 0x" << std::hex << v;
    }
}

TEST(SoftI2C, ClockStretchingIsTolerated)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[2] = {0};
    setup(&fb, &bus);

    fb.mem[0x30] = 0xA5; fb.mem[0x31] = 0x5A;
    fb.stretch_us = 200;        /* slave stalls the clock for 200 us */
    fb.stretch_remaining = 1;

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x30, buf, 2), SOFT_I2C_OK)
        << "stretching must not break the transfer";
    EXPECT_EQ(buf[0], 0xA5);
    EXPECT_EQ(buf[1], 0x5A);
}

TEST(SoftI2C, StretchBeyondTimeoutReportsTimeout)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[1] = {0};
    setup(&fb, &bus);

    fb.stretch_us = 50000;      /* far past the 2000 us limit */
    fb.stretch_remaining = 1;

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x00, buf, 1), SOFT_I2C_ERR_TIMEOUT)
        << "a hung slave must surface as TIMEOUT, not a hang";
}

TEST(SoftI2C, DataNackIsReported)
{
    fake_bus_t fb; soft_i2c_t bus;
    const uint8_t payload[] = { 1, 2, 3, 4 };
    setup(&fb, &bus);

    fb.nack_after_bytes = 2;    /* accept reg ptr + 1 byte, then refuse */

    EXPECT_EQ(soft_i2c_mem_write(&bus, SLAVE_ADDR, 0x00, payload, 4), SOFT_I2C_ERR_NACK_DATA)
        << "mid-transfer NACK must be reported";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "STOP must still be issued after a NACK";
}

TEST(SoftI2C, RecoverFreesJammedSda)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    fb.jam_sda = true;
    fb.jam_release_after = 3;   /* frees itself after 3 clocks */

    EXPECT_EQ(soft_i2c_recover(&bus), SOFT_I2C_OK) << "recovery should free the bus";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "bus idle after recovery";
}

TEST(SoftI2C, RecoverGivesUpOnPermanentJam)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    fb.jam_sda = true;
    fb.jam_release_after = 0;   /* never releases */

    EXPECT_EQ(soft_i2c_recover(&bus), SOFT_I2C_ERR_BUS_BUSY)
        << "permanently jammed bus must report BUS_BUSY";
}

TEST(SoftI2C, StartOnBusyBusIsRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t b = 0;
    setup(&fb, &bus);

    fb.jam_sda = true;
    fb.jam_release_after = 0;

    EXPECT_EQ(soft_i2c_read(&bus, SLAVE_ADDR, &b, 1), SOFT_I2C_ERR_BUS_BUSY)
        << "must not attempt START when SDA is held low";
}

TEST(SoftI2C, BitCountIsNinePerByte)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    soft_i2c_probe(&bus, SLAVE_ADDR);
    /* 8 data bits + 1 ACK bit = 9, plus one more rise for the STOP
     * condition, which needs SCL high before SDA is released. */
    EXPECT_EQ(fake_bus_count_scl_rises(&fb), 10u)
        << "a probe must produce 9 clock pulses plus the STOP rise";
}

TEST(SoftI2C, SclHighTimeRespectsHalfPeriod)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[2];
    setup(&fb, &bus);

    soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x00, buf, 2);
    EXPECT_GE(fake_bus_min_scl_high_us(&fb), 5u)
        << "SCL high time must be at least the configured half period";
}

TEST(SoftI2C, NullPortRejected)
{
    soft_i2c_t bus;
    soft_i2c_port_t port;
    memset(&port, 0, sizeof(port));

    EXPECT_EQ(soft_i2c_init(&bus, &port, 5, 1000), SOFT_I2C_ERR_PARAM)
        << "incomplete port must be rejected";
    EXPECT_EQ(soft_i2c_init(NULL, NULL, 5, 1000), SOFT_I2C_ERR_PARAM)
        << "null args must be rejected";
}

/* ---------------------------------------------------------------- */
/* init param validation                                             */

TEST(SoftI2C, ZeroHalfPeriodRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    soft_i2c_port_t port;
    fake_bus_init(&fb);
    fake_bus_attach_slave(&fb, SLAVE_ADDR);
    port = fake_bus_port(&fb);

    EXPECT_EQ(soft_i2c_init(&bus, &port, 0, 1000), SOFT_I2C_ERR_PARAM)
        << "zero half_period_us must be rejected";
}

/* ---------------------------------------------------------------- */
/* raw write/read                                                    */

TEST(SoftI2C, WriteLenZeroSucceedsNoDataPhase)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_write(&bus, SLAVE_ADDR, NULL, 0), SOFT_I2C_OK)
        << "zero-length write should succeed with no data phase";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "bus idle after zero-length write";
    EXPECT_EQ(fake_bus_count_scl_rises(&fb), 10u)
        << "only address byte + ACK + STOP rise, no data byte";
}

TEST(SoftI2C, WriteNullDataWithLenRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_write(&bus, SLAVE_ADDR, NULL, 3), SOFT_I2C_ERR_PARAM)
        << "NULL data with len>0 must be rejected";
    EXPECT_EQ(fb.log_len, 0u) << "must reject before touching the bus at all";
}

TEST(SoftI2C, ReadLenZeroRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[1];
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_read(&bus, SLAVE_ADDR, buf, 0), SOFT_I2C_ERR_PARAM)
        << "zero-length read must be rejected";
    EXPECT_EQ(fb.log_len, 0u) << "must reject before touching the bus at all";
}

TEST(SoftI2C, ReadNullDataRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_read(&bus, SLAVE_ADDR, NULL, 1), SOFT_I2C_ERR_PARAM)
        << "NULL data buffer must be rejected";
    EXPECT_EQ(fb.log_len, 0u) << "must reject before touching the bus at all";
}

TEST(SoftI2C, WriteThenReadRawRoundtrip)
{
    fake_bus_t fb; soft_i2c_t bus;
    const uint8_t payload[] = { 0x05, 0xAA, 0xBB };
    uint8_t buf[2] = {0};

    setup(&fb, &bus);
    EXPECT_EQ(soft_i2c_write(&bus, SLAVE_ADDR, payload, 3), SOFT_I2C_OK)
        << "raw write should succeed";
    EXPECT_EQ(fb.mem[0x05], 0xAA) << "byte after the pointer stored at reg 0x05";
    EXPECT_EQ(fb.mem[0x06], 0xBB) << "next byte stored at reg 0x06";

    setup(&fb, &bus);
    fb.mem[0x00] = 0x11; fb.mem[0x01] = 0x22;
    EXPECT_EQ(soft_i2c_read(&bus, SLAVE_ADDR, buf, 2), SOFT_I2C_OK)
        << "raw read should succeed";
    EXPECT_EQ(buf[0], 0x11);
    EXPECT_EQ(buf[1], 0x22);
}

/* ---------------------------------------------------------------- */
/* mem_write/mem_read param validation                               */

TEST(SoftI2C, MemWriteLenZeroWritesOnlyRegisterPointer)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_mem_write(&bus, SLAVE_ADDR, 0x10, NULL, 0), SOFT_I2C_OK)
        << "zero-length mem_write should still send the register pointer";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "bus idle after zero-length mem_write";
}

TEST(SoftI2C, MemWriteNullDataWithLenRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_mem_write(&bus, SLAVE_ADDR, 0x10, NULL, 2), SOFT_I2C_ERR_PARAM)
        << "NULL data with len>0 must be rejected";
    EXPECT_EQ(fb.log_len, 0u) << "must reject before touching the bus at all";
}

TEST(SoftI2C, MemReadLenZeroRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[1];
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x10, buf, 0), SOFT_I2C_ERR_PARAM)
        << "zero-length mem_read must be rejected";
    EXPECT_EQ(fb.log_len, 0u) << "must reject before touching the bus at all";
}

TEST(SoftI2C, MemReadNullDataRejected)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x10, NULL, 2), SOFT_I2C_ERR_PARAM)
        << "NULL data buffer must be rejected";
    EXPECT_EQ(fb.log_len, 0u) << "must reject before touching the bus at all";
}

TEST(SoftI2C, MemReadPhase1NackAbortsBeforeRepeatedStart)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[2] = {0};
    setup(&fb, &bus);

    fb.nack_after_bytes = 0;   /* NACK the very first RX byte: the reg pointer */

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x10, buf, 2), SOFT_I2C_ERR_NACK_DATA)
        << "a NACKed register pointer must abort before the repeated START";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "STOP must still be issued";
    EXPECT_EQ(fb.start_count, 1u) << "phase 2's repeated START must never have been sent";
}

/* ---------------------------------------------------------------- */
/* arbitration loss                                                  */

TEST(SoftI2C, ProbeReportsArbLostOnFirstBit)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    fb.arb_jam_at_rise = 1;    /* contend on the address byte's first (MSB) bit */

    EXPECT_EQ(soft_i2c_probe(&bus, 0x7F), SOFT_I2C_ERR_ARB_LOST)
        << "another master pulling SDA low must be reported as ARB_LOST";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "bus idle after losing arbitration";
}

TEST(SoftI2C, WriteReportsArbLostMidDataPhase)
{
    fake_bus_t fb; soft_i2c_t bus;
    const uint8_t data = 0xFF;
    setup(&fb, &bus);

    fb.arb_jam_at_rise = 10;  /* address byte (8) + its ACK (1) = 9 rises, then data bit 1 */

    EXPECT_EQ(soft_i2c_write(&bus, SLAVE_ADDR, &data, 1), SOFT_I2C_ERR_ARB_LOST)
        << "arbitration loss must be detected in the data phase too";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "bus idle after losing arbitration";
}

TEST(SoftI2C, ArbJamOnZeroBitHasNoEffect)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    fb.arb_jam_at_rise = 2;   /* SLAVE_ADDR (0x50) -> address byte 0xA0: bit 2 is 0 */

    EXPECT_EQ(soft_i2c_probe(&bus, SLAVE_ADDR), SOFT_I2C_OK)
        << "forcing SDA low on a bit we're already driving low must not misfire ARB_LOST";
}

/* ---------------------------------------------------------------- */
/* mem_read address-phase NACKs                                      */

TEST(SoftI2C, MemReadPhase1AddrNackReported)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[2] = { 0xEE, 0xEE };
    setup(&fb, &bus);

    fb.nack_addr_at_start = 1;   /* NACK the very first address phase */

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x10, buf, 2), SOFT_I2C_ERR_NACK_ADDR)
        << "a NACKed initial address must be reported";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "STOP must still be issued";
}

TEST(SoftI2C, MemReadPhase2AddrNackReported)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[2] = { 0xEE, 0xEE };
    setup(&fb, &bus);

    fb.nack_addr_at_start = 2;   /* NACK only the repeated START's address */

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x10, buf, 2), SOFT_I2C_ERR_NACK_ADDR)
        << "a NACKed repeated-START address must be reported";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "STOP must still be issued";
    EXPECT_EQ(buf[0], 0xEE);
    EXPECT_EQ(buf[1], 0xEE);
}

/* ---------------------------------------------------------------- */
/* boundary cases                                                    */

/* The stretch is armed before send_start()'s trailing wait and the first
 * bit's setup-time wait run (5us each), which drain the stretch budget in
 * two 5us steps before scl_rise()'s own 1us-granular timeout loop starts
 * counting - so the real boundary sits 10us above stretch_timeout_us (2000
 * in `setup()`), not at 2000 itself. Verified empirically: 2010 succeeds,
 * 2011 times out. */
TEST(SoftI2C, StretchExactlyAtTimeoutSucceeds)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[1] = {0};
    setup(&fb, &bus);

    fb.stretch_us = 2010;
    fb.stretch_remaining = 1;

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x00, buf, 1), SOFT_I2C_OK)
        << "a stretch exactly at the timeout boundary must still succeed";
}

TEST(SoftI2C, StretchOneUsPastTimeoutReportsTimeout)
{
    fake_bus_t fb; soft_i2c_t bus;
    uint8_t buf[1] = {0};
    setup(&fb, &bus);

    fb.stretch_us = 2011;
    fb.stretch_remaining = 1;

    EXPECT_EQ(soft_i2c_mem_read(&bus, SLAVE_ADDR, 0x00, buf, 1), SOFT_I2C_ERR_TIMEOUT)
        << "a stretch one microsecond past the boundary must report TIMEOUT";
}

TEST(SoftI2C, RecoverBoundaryReleasesOnLastPulse)
{
    fake_bus_t fb; soft_i2c_t bus;
    setup(&fb, &bus);

    fb.jam_sda = true;
    fb.jam_release_after = 9;  /* releases on the very last of the 9 recovery clocks */

    EXPECT_EQ(soft_i2c_recover(&bus), SOFT_I2C_OK)
        << "recovery must succeed when SDA frees on the final clock";
    EXPECT_TRUE(fake_bus_is_idle(&fb)) << "bus idle after recovery";
}

TEST(SoftI2C, ProbeAddrExtremes)
{
    fake_bus_t fb; soft_i2c_t bus;
    soft_i2c_port_t port;

    fake_bus_init(&fb);
    fake_bus_attach_slave(&fb, 0x00);
    port = fake_bus_port(&fb);
    soft_i2c_init(&bus, &port, 5, 2000);
    EXPECT_EQ(soft_i2c_probe(&bus, 0x00), SOFT_I2C_OK) << "address 0x00 must be reachable";

    fake_bus_init(&fb);
    fake_bus_attach_slave(&fb, 0x7F);
    port = fake_bus_port(&fb);
    soft_i2c_init(&bus, &port, 5, 2000);
    EXPECT_EQ(soft_i2c_probe(&bus, 0x7F), SOFT_I2C_OK) << "address 0x7F must be reachable";

    setup(&fb, &bus);   /* slave at SLAVE_ADDR (0x50) */
    EXPECT_EQ(soft_i2c_probe(&bus, 0x00), SOFT_I2C_ERR_NACK_ADDR) << "0x00 must NACK when unattached";
    EXPECT_EQ(soft_i2c_probe(&bus, 0x7F), SOFT_I2C_ERR_NACK_ADDR) << "0x7F must NACK when unattached";
}
