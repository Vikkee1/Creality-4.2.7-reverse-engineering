#include "mock_bus.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Slave state machine                                                  */
/* ------------------------------------------------------------------ */

static void slave_on_start(fake_bus_t *b)
{
    /* START or repeated START: abandon whatever we were doing. */
    b->start_count++;
    b->s_sda_low = false;
    b->state = b->slave_present ? SL_ADDR : SL_IGNORE;
    b->shift = 0;
    b->bit_count = 0;
}

static void slave_on_stop(fake_bus_t *b)
{
    b->s_sda_low = false;
    b->state = SL_IDLE;
    b->bit_count = 0;
    b->reg_ptr_set = false;
    b->bytes_seen = 0;
}

static void slave_on_scl_rise(fake_bus_t *b)
{
    /* Data is sampled while the clock is high. */
    switch (b->state) {
    case SL_ADDR:
    case SL_RX:
        b->shift = (uint8_t)((b->shift << 1) | (b->sda ? 1u : 0u));
        b->bit_count++;
        break;
    case SL_TX_ACK:
        b->master_acked = !b->sda;   /* low = ACK */
        break;
    default:
        break;
    }
}

static void slave_on_scl_fall(fake_bus_t *b)
{
    /* Data changes while the clock is low. */
    switch (b->state) {
    case SL_ADDR:
        if (b->bit_count == 8) {
            uint8_t addr = (uint8_t)(b->shift >> 1);
            bool    read = (b->shift & 1u) != 0u;
            bool    forced_nack = (b->nack_addr_at_start >= 0 &&
                                    (int)b->start_count == b->nack_addr_at_start);
            if (addr == b->slave_addr7 && !forced_nack) {
                b->s_sda_low = true;          /* ACK the address */
                b->state = SL_ADDR_ACK;
                b->master_acked = read;       /* remember direction */
            } else {
                b->state = SL_IGNORE;         /* NACK by staying quiet */
            }
        }
        break;

    case SL_ADDR_ACK:
        b->s_sda_low = false;                 /* release after ACK */
        b->bit_count = 0;
        if (b->master_acked) {                /* read transaction */
            b->shift = b->mem[b->reg_ptr];
            b->reg_ptr = (uint8_t)(b->reg_ptr + 1);
            b->state = SL_TX;
            /* Present bit 7 on this same falling edge. */
            b->s_sda_low = (b->shift & 0x80u) == 0u;
            b->shift = (uint8_t)(b->shift << 1);
            b->bit_count = 1;
        } else {
            b->state = SL_RX;
        }
        break;

    case SL_RX:
        if (b->bit_count == 8) {
            bool nack = (b->nack_after_bytes >= 0 &&
                         b->bytes_seen >= b->nack_after_bytes);
            if (!b->reg_ptr_set) {
                b->reg_ptr = b->shift;
                b->reg_ptr_set = true;
            } else {
                b->mem[b->reg_ptr] = b->shift;
                b->reg_ptr = (uint8_t)(b->reg_ptr + 1);
            }
            b->bytes_seen++;
            if (!nack) {
                b->s_sda_low = true;          /* ACK the byte */
            }
            b->state = SL_RX_ACK;
        }
        break;

    case SL_RX_ACK:
        b->s_sda_low = false;
        b->bit_count = 0;
        b->shift = 0;
        b->state = SL_RX;
        break;

    case SL_TX:
        if (b->bit_count < 8) {
            b->s_sda_low = (b->shift & 0x80u) == 0u;
            b->shift = (uint8_t)(b->shift << 1);
            b->bit_count++;
        } else {
            b->s_sda_low = false;             /* release for master's ACK */
            b->state = SL_TX_ACK;
        }
        break;

    case SL_TX_ACK:
        if (b->master_acked) {
            b->shift = b->mem[b->reg_ptr];
            b->reg_ptr = (uint8_t)(b->reg_ptr + 1);
            b->s_sda_low = (b->shift & 0x80u) == 0u;
            b->shift = (uint8_t)(b->shift << 1);
            b->bit_count = 1;
            b->state = SL_TX;
        } else {
            b->s_sda_low = false;
            b->state = SL_IGNORE;             /* wait for STOP */
        }
        break;

    default:
        break;
    }

    /* Optional clock stretching, applied after the FSM has settled. */
    if (b->stretch_remaining > 0) {
        b->s_scl_low = true;
        b->stretch_remaining--;
    }

    /* Optional SDA jam, released after N falling edges. */
    if (b->jam_sda) {
        if (b->jam_release_after > 0) {
            b->jam_release_after--;
            if (b->jam_release_after == 0) b->jam_sda = false;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Line resolution and edge detection                                   */
/* ------------------------------------------------------------------ */

static void log_event(fake_bus_t *b)
{
    if (!b->log_enabled || b->log_len >= FAKE_LOG_SIZE) return;
    b->log[b->log_len].t_us = b->now_us;
    b->log[b->log_len].scl  = b->scl;
    b->log[b->log_len].sda  = b->sda;
    b->log_len++;
}

static void bus_settle(fake_bus_t *b)
{
    bool prev_scl = b->scl;
    bool prev_sda = b->sda;

    /* Wired AND: high only if nobody pulls low. */
    bool scl = !(b->m_scl_low || b->s_scl_low);
    bool sda = !(b->m_sda_low || b->s_sda_low || b->jam_sda);

    b->scl = scl;
    b->sda = sda;

    if (scl && prev_scl && sda != prev_sda) {
        if (prev_sda && !sda) slave_on_start(b);
        else                  slave_on_stop(b);
    }
    bool rising = (!prev_scl && scl);
    if (rising) slave_on_scl_rise(b);
    if (prev_scl && !scl) slave_on_scl_fall(b);

    /* The FSM may have changed the slave's drivers; recompute quietly. */
    b->scl = !(b->m_scl_low || b->s_scl_low);
    b->sda = !(b->m_sda_low || b->s_sda_low || b->jam_sda);

    if (rising) {
        /* One-shot: overrides only this call's local `sda`, as if a third
         * bus participant contended on this edge. Not folded into the
         * persistent drivers, so it self-clears on the very next call. */
        b->scl_rise_count++;
        if ((int)b->scl_rise_count == b->arb_jam_at_rise) {
            b->sda = false;
        }
    }

    if (b->scl != prev_scl || b->sda != prev_sda) log_event(b);
}

/* ------------------------------------------------------------------ */
/* Port implementation                                                  */
/* ------------------------------------------------------------------ */

static void fake_sda_set(void *ctx, bool release)
{
    fake_bus_t *b = (fake_bus_t *)ctx;
    b->m_sda_low = !release;
    bus_settle(b);
}

static void fake_scl_set(void *ctx, bool release)
{
    fake_bus_t *b = (fake_bus_t *)ctx;
    b->m_scl_low = !release;
    bus_settle(b);
}

static bool fake_sda_get(void *ctx)
{
    fake_bus_t *b = (fake_bus_t *)ctx;
    return b->sda;
}

static bool fake_scl_get(void *ctx)
{
    fake_bus_t *b = (fake_bus_t *)ctx;
    return b->scl;
}

static void fake_delay(void *ctx, uint32_t us)
{
    fake_bus_t *b = (fake_bus_t *)ctx;
    b->now_us += us;

    /* Time passing is what ends a stretch. */
    if (b->s_scl_low && b->stretch_us > 0) {
        if (b->stretch_us <= us) {
            b->stretch_us = 0;
            b->s_scl_low = false;
            bus_settle(b);
        } else {
            b->stretch_us -= us;
        }
    }
}

soft_i2c_port_t fake_bus_port(fake_bus_t *b)
{
    soft_i2c_port_t p;
    p.sda_set  = fake_sda_set;
    p.scl_set  = fake_scl_set;
    p.sda_get  = fake_sda_get;
    p.scl_get  = fake_scl_get;
    p.delay_us = fake_delay;
    p.ctx      = b;
    return p;
}

/* ------------------------------------------------------------------ */
/* Setup and analysis                                                   */
/* ------------------------------------------------------------------ */

void fake_bus_init(fake_bus_t *b)
{
    memset(b, 0, sizeof(*b));
    b->sda = true;              /* pull-ups hold both lines high */
    b->scl = true;
    b->state = SL_IDLE;
    b->nack_after_bytes = -1;
    b->arb_jam_at_rise = -1;
    b->nack_addr_at_start = -1;
    b->log_enabled = true;
}

void fake_bus_attach_slave(fake_bus_t *b, uint8_t addr7)
{
    b->slave_present = true;
    b->slave_addr7 = addr7;
}

size_t fake_bus_count_scl_rises(const fake_bus_t *b)
{
    size_t n = 0;
    bool prev = true;
    for (size_t i = 0; i < b->log_len; i++) {
        if (!prev && b->log[i].scl) n++;
        prev = b->log[i].scl;
    }
    return n;
}

uint32_t fake_bus_min_scl_high_us(const fake_bus_t *b)
{
    uint32_t min = 0xFFFFFFFFu;
    uint32_t rise_t = 0;
    bool have_rise = false;
    bool prev = true;

    for (size_t i = 0; i < b->log_len; i++) {
        if (!prev && b->log[i].scl) { rise_t = b->log[i].t_us; have_rise = true; }
        if (prev && !b->log[i].scl && have_rise) {
            uint32_t d = b->log[i].t_us - rise_t;
            if (d < min) min = d;
            have_rise = false;
        }
        prev = b->log[i].scl;
    }
    return min;
}

bool fake_bus_is_idle(const fake_bus_t *b)
{
    return b->sda && b->scl;
}