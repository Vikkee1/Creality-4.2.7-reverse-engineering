#include "soft_i2c.h"
 
/* ---- Thin wrappers so the body reads like protocol, not plumbing ---- */
static inline void sda_release(soft_i2c_t *b) { b->port.sda_set(b->port.ctx, true); }
static inline void sda_low(soft_i2c_t *b)     { b->port.sda_set(b->port.ctx, false); }
static inline void scl_low(soft_i2c_t *b)     { b->port.scl_set(b->port.ctx, false); }
static inline bool sda_read(soft_i2c_t *b)    { return b->port.sda_get(b->port.ctx); }
static inline void wait(soft_i2c_t *b)        { b->port.delay_us(b->port.ctx, b->half_period_us); }

/**
 * Release SCL and wait for it to rise.
 * @param *b context pointer.
 * @return SOFT_I2C_OK when succesfull. SOFT_I2C_ERR_TIMEOUT when timeout
 * triggered.
 */
static soft_i2c_status_t scl_rise(soft_i2c_t *b){
    uint32_t waited = 0;

    b->port.scl_set(b->port.ctx, true);

    while (!b->port.scl_get(b->port.ctx)){
        b->port.delay_us(b->port.ctx, 1);
        if(++waited > b->stretch_timeout_us){
            return SOFT_I2C_ERR_TIMEOUT;
        }
    }
    return SOFT_I2C_OK;
}

static soft_i2c_status_t write_bit(soft_i2c_t *b, bool bit){
    soft_i2c_status_t st;

    // Write 1 or 0
    if (bit) sda_release(b); else sda_low(b);

    // Setup time
    wait(b);

    // SCL rise
    st = scl_rise(b);
    if (st != SOFT_I2C_OK) return st;

    // Check SDA state if another master is driving the line
    if (bit && !sda_read(b)){
        return SOFT_I2C_ERR_ARB_LOST;
    }

    wait(b);

    scl_low(b);

    return SOFT_I2C_OK;
}

static soft_i2c_status_t read_bit(soft_i2c_t *b, bool *bit){

    soft_i2c_status_t st;

    // Release SDA
    sda_release(b);
    wait(b);

    // Release and wait for rise
    st = scl_rise(b);

    if (st != SOFT_I2C_OK) return st;

    wait(b);

    *bit = sda_read(b);

    scl_low(b);

    return SOFT_I2C_OK;
}

/* ---- START / STOP ---- */
 
/* SDA falls while SCL is high. Written to work as repeated START too. */
static soft_i2c_status_t send_start(soft_i2c_t *b){

    soft_i2c_status_t st;

    sda_release(b);
    wait(b);

    st = scl_rise(b);
    if (st != SOFT_I2C_OK) return st;
    wait(b);

    // Check if another driving the bus
    if (!sda_read(b)) return SOFT_I2C_ERR_BUS_BUSY;

    sda_low(b);
    wait(b);
    scl_low(b);
    wait(b);

    return SOFT_I2C_OK;
}

/* SDA  rises while SCL SCL is high. */
static soft_i2c_status_t send_stop(soft_i2c_t *b){

    soft_i2c_status_t st;

    sda_low(b);
    wait(b);

    st = scl_rise(b);
    if (st != SOFT_I2C_OK) return st;
    wait(b);

    sda_release(b);
    wait(b);

    return SOFT_I2C_OK;
}

/* ---- Byte level ---- */
 
static soft_i2c_status_t write_byte(soft_i2c_t *b, uint8_t byte, bool *acked)
{
    soft_i2c_status_t st;
    bool nack;
 
    for (uint8_t i = 0; i < 8; i++) {
        st = write_bit(b, (byte & 0x80u) != 0u);
        if (st != SOFT_I2C_OK) return st;
        byte = (uint8_t)(byte << 1u);
    }
 
    /* 9th clock: release SDA, the slave pulls it low to ACK. */
    st = read_bit(b, &nack);
    if (st != SOFT_I2C_OK) return st;
 
    *acked = !nack;
    return SOFT_I2C_OK;
}

static soft_i2c_status_t read_byte(soft_i2c_t *b, uint8_t *out, bool ack)
{
    soft_i2c_status_t st;
    uint8_t byte = 0;
    bool bit;
 
    for (uint8_t i = 0; i < 8; i++) {
        st = read_bit(b, &bit);
        if (st != SOFT_I2C_OK) return st;
        byte = (uint8_t)((byte << 1u) | (bit ? 1u : 0u));
    }
 
    /* ACK asks for another byte; NACK ends the read. */
    st = write_bit(b, !ack);
    if (st != SOFT_I2C_OK) return st;
 
    *out = byte;
    return SOFT_I2C_OK;
}

/* ---- Public API ---- */

soft_i2c_status_t soft_i2c_init(soft_i2c_t *bus,
                                const soft_i2c_port_t *port,
                                uint32_t half_period_us,
                                uint32_t stretch_timeout_us)
{
    if (bus == NULL || port == NULL) return SOFT_I2C_ERR_PARAM;
    
    if( port->sda_set == NULL || port->scl_set == NULL ||
        port->sda_get == NULL || port->scl_get == NULL ||
        port->delay_us == NULL){
            return SOFT_I2C_ERR_PARAM;
        }

    if (half_period_us == 0) return SOFT_I2C_ERR_PARAM;

    bus->port = *port;
    bus->half_period_us = half_period_us;
    bus->stretch_timeout_us = stretch_timeout_us;

    sda_release(bus);
    bus->port.scl_set(bus->port.ctx, true);

    return SOFT_I2C_OK;
}

soft_i2c_status_t soft_i2c_recover(soft_i2c_t *bus)
{
    sda_release(bus);
    bus->port.scl_set(bus->port.ctx, true);
    wait(bus);
 
    if (sda_read(bus)) {
        return SOFT_I2C_OK;   /* already idle */
    }
 
    for (uint8_t i = 0; i < 9u; i++) {
        scl_low(bus);
        wait(bus);
        bus->port.scl_set(bus->port.ctx, true);
        wait(bus);
        if (sda_read(bus)) break;
    }
 
    (void)send_stop(bus);
    return sda_read(bus) ? SOFT_I2C_OK : SOFT_I2C_ERR_BUS_BUSY;
}

/* Address phase shared by every transaction below. */
static soft_i2c_status_t addr_phase(soft_i2c_t *bus, uint8_t addr7, bool read)
{
    soft_i2c_status_t st;
    bool acked;
 
    st = write_byte(bus, (uint8_t)((addr7 << 1u) | (read ? 1u : 0u)), &acked);
    if (st != SOFT_I2C_OK) return st;
 
    return acked ? SOFT_I2C_OK : SOFT_I2C_ERR_NACK_ADDR;
}

soft_i2c_status_t soft_i2c_probe(soft_i2c_t *bus, uint8_t addr7)
{
    soft_i2c_status_t st = send_start(bus);
    if (st != SOFT_I2C_OK) return st;
 
    st = addr_phase(bus, addr7, false);
    (void)send_stop(bus);
    return st;
}

soft_i2c_status_t soft_i2c_write(soft_i2c_t *bus, uint8_t addr7,
                                 const uint8_t *data, size_t len)
{
    soft_i2c_status_t st;
    bool acked;
 
    if (len > 0 && data == NULL) return SOFT_I2C_ERR_PARAM;
 
    st = send_start(bus);
    if (st != SOFT_I2C_OK) return st;
 
    st = addr_phase(bus, addr7, false);
    if (st != SOFT_I2C_OK) goto done;
 
    for (size_t i = 0; i < len; i++) {
        st = write_byte(bus, data[i], &acked);
        if (st != SOFT_I2C_OK) goto done;
        if (!acked) { st = SOFT_I2C_ERR_NACK_DATA; goto done; }
    }
 
done:
    (void)send_stop(bus);
    return st;
}

soft_i2c_status_t soft_i2c_read(soft_i2c_t *bus, uint8_t addr7,
                                uint8_t *data, size_t len)
{
    soft_i2c_status_t st;
 
    if (len == 0 || data == NULL) return SOFT_I2C_ERR_PARAM;
 
    st = send_start(bus);
    if (st != SOFT_I2C_OK) return st;
 
    st = addr_phase(bus, addr7, true);
    if (st != SOFT_I2C_OK) goto done;
 
    for (size_t i = 0; i < len; i++) {
        st = read_byte(bus, &data[i], i < (len - 1));   /* NACK the last */
        if (st != SOFT_I2C_OK) goto done;
    }
 
done:
    (void)send_stop(bus);
    return st;
}

soft_i2c_status_t soft_i2c_mem_write(soft_i2c_t *bus, uint8_t addr7,
                                     uint8_t reg, const uint8_t *data,
                                     size_t len)
{
    soft_i2c_status_t st;
    bool acked;
 
    if (len > 0 && data == NULL) return SOFT_I2C_ERR_PARAM;
 
    st = send_start(bus);
    if (st != SOFT_I2C_OK) return st;
 
    st = addr_phase(bus, addr7, false);
    if (st != SOFT_I2C_OK) goto done;
 
    st = write_byte(bus, reg, &acked);
    if (st != SOFT_I2C_OK) goto done;
    if (!acked) { st = SOFT_I2C_ERR_NACK_DATA; goto done; }
 
    for (size_t i = 0; i < len; i++) {
        st = write_byte(bus, data[i], &acked);
        if (st != SOFT_I2C_OK) goto done;
        if (!acked) { st = SOFT_I2C_ERR_NACK_DATA; goto done; }
    }
 
done:
    (void)send_stop(bus);
    return st;
}

soft_i2c_status_t soft_i2c_mem_read(soft_i2c_t *bus, uint8_t addr7,
                                    uint8_t reg, uint8_t *data, size_t len)
{
    soft_i2c_status_t st;
    bool acked;
 
    if (len == 0 || data == NULL) return SOFT_I2C_ERR_PARAM;
 
    /* Phase 1: point the slave at the register. No STOP. */
    st = send_start(bus);
    if (st != SOFT_I2C_OK) return st;
 
    st = addr_phase(bus, addr7, false);
    if (st != SOFT_I2C_OK) goto done;
 
    st = write_byte(bus, reg, &acked);
    if (st != SOFT_I2C_OK) goto done;
    if (!acked) { st = SOFT_I2C_ERR_NACK_DATA; goto done; }
 
    /* Phase 2: repeated START, then read. */
    st = send_start(bus);
    if (st != SOFT_I2C_OK) goto done;
 
    st = addr_phase(bus, addr7, true);
    if (st != SOFT_I2C_OK) goto done;
 
    for (size_t i = 0; i < len; i++) {
        st = read_byte(bus, &data[i], i < (len - 1));
        if (st != SOFT_I2C_OK) goto done;
    }
 
done:
    (void)send_stop(bus);
    return st;
}