/*
 * fake_bus.h — Host-side simulation of an I2C bus.
 *
 * Models the wired-AND behaviour of a real open-drain bus: the line is
 * high only when nobody is pulling it down. A slave state machine
 * decodes the master's waveform, so the tests exercise real protocol,
 * not a stubbed-out "return 0x42" mock.
 */
#ifndef FAKE_BUS_H
#define FAKE_BUS_H

#include "soft_i2c.h"

#define FAKE_MEM_SIZE   256
#define FAKE_LOG_SIZE   8192

typedef enum {
    SL_IDLE = 0,
    SL_ADDR,      /* shifting in address byte      */
    SL_ADDR_ACK,  /* driving ACK for the address   */
    SL_RX,        /* shifting in a data byte       */
    SL_RX_ACK,    /* driving ACK for a data byte   */
    SL_TX,        /* shifting out a data byte      */
    SL_TX_ACK,    /* reading the master's ACK/NACK */
    SL_IGNORE,    /* not our address; stay quiet   */
} slave_state_t;

typedef struct {
    uint32_t t_us;
    bool scl;
    bool sda;
} bus_event_t;

typedef struct {
    /* --- Line drivers. true means "pulling low". --- */
    bool m_sda_low, m_scl_low;
    bool s_sda_low, s_scl_low;

    /* --- Resulting line levels --- */
    bool sda, scl;

    /* --- Virtual time --- */
    uint32_t now_us;

    /* --- Slave model --- */
    bool     slave_present;
    uint8_t  slave_addr7;
    uint8_t  mem[FAKE_MEM_SIZE];
    uint8_t  reg_ptr;
    bool     reg_ptr_set;      /* first write byte is the register pointer */
    slave_state_t state;
    uint8_t  shift;
    int      bit_count;
    bool     master_acked;

    /* --- Fault injection --- */
    int      nack_after_bytes;   /* -1 = never; else NACK the Nth data byte */
    int      bytes_seen;
    uint32_t stretch_us;         /* hold SCL low this long each falling edge */
    int      stretch_remaining;  /* how many more edges to stretch           */
    bool     jam_sda;            /* slave holds SDA low permanently          */
    int      jam_release_after;  /* release jam after N SCL falling edges    */

    int      arb_jam_at_rise;    /* -1 = never; else force SDA low on this
                                   * 1-based live SCL rising-edge count, as if
                                   * a third bus participant were contending */
    uint32_t scl_rise_count;     /* live count of SCL low->high transitions */

    int      nack_addr_at_start; /* -1 = never; else force an address NACK
                                   * on this 1-based START/repeated-START
                                   * occurrence, regardless of address match */
    uint32_t start_count;        /* live count of START conditions seen     */

    /* --- Waveform log --- */
    bus_event_t log[FAKE_LOG_SIZE];
    size_t   log_len;
    bool     log_enabled;
} fake_bus_t;

#ifdef __cplusplus
extern "C" {
#endif

void fake_bus_init(fake_bus_t *b);
void fake_bus_attach_slave(fake_bus_t *b, uint8_t addr7);
soft_i2c_port_t fake_bus_port(fake_bus_t *b);

/* Analysis helpers for assertions. */
size_t   fake_bus_count_scl_rises(const fake_bus_t *b);
uint32_t fake_bus_min_scl_high_us(const fake_bus_t *b);
bool     fake_bus_is_idle(const fake_bus_t *b);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_BUS_H */