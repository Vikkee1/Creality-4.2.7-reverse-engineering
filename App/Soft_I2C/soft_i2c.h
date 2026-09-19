/*
 * soft_i2c.h — Hardware-independent bit-banged I2C master.
 *
 * No HAL, no CMSIS, no target headers. It compiles for the host as-is.
 */
#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOFT_I2C_OK = 0,
    SOFT_I2C_ERR_NACK_ADDR,   /* No device responded to the address */
    SOFT_I2C_ERR_NACK_DATA,   /* Device stopped accepting mid-transfer */
    SOFT_I2C_ERR_TIMEOUT,     /* Slave stretched SCL past the limit */
    SOFT_I2C_ERR_BUS_BUSY,    /* SDA held low by someone else at START */
    SOFT_I2C_ERR_ARB_LOST,    /* We released SDA but read it low */
    SOFT_I2C_ERR_PARAM,
} soft_i2c_status_t;

/*
 * The port. Five functions, all trivially fakeable.
 *
 * IMPORTANT semantics: `release` mirrors open-drain hardware.
 *   release = true  -> stop driving, let the pull-up win (logical 1)
 *   release = false -> drive the line to ground (logical 0)
 * A port must NEVER actively drive a line high.
 */
typedef struct {
    void (*sda_set)(void *ctx, bool release);
    void (*scl_set)(void *ctx, bool release);
    bool (*sda_get)(void *ctx);
    bool (*scl_get)(void *ctx);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} soft_i2c_port_t;

typedef struct {
    soft_i2c_port_t port;
    uint32_t half_period_us;      /* 5 -> ~100 kHz */
    uint32_t stretch_timeout_us;  /* Give up if SCL stays low this long */
} soft_i2c_t;

/**
 * Software I2C initialization.
 * @param *bus Pointer to bus configuration structure.
 * @param *port Pointer to host port structure.
 * @param half_period_us Half period time in us. 
 * @param stretch_timeout_us Maximum clock stretching timeout in us.
 * @return SOFT_I2C_OK when succesfull.
 */
soft_i2c_status_t soft_i2c_init(soft_i2c_t *bus,
                                const soft_i2c_port_t *port,
                                uint32_t half_period_us,
                                uint32_t stretch_timeout_us);

/* Clock out up to 9 pulses to free a slave stuck holding SDA low. */
soft_i2c_status_t soft_i2c_recover(soft_i2c_t *bus);

/* Address-only transaction. Returns OK if something ACKs. */
soft_i2c_status_t soft_i2c_probe(soft_i2c_t *bus, uint8_t addr7);

soft_i2c_status_t soft_i2c_write(soft_i2c_t *bus, uint8_t addr7,
                                 const uint8_t *data, size_t len);

soft_i2c_status_t soft_i2c_read(soft_i2c_t *bus, uint8_t addr7,
                                uint8_t *data, size_t len);

/* Write register pointer, repeated START, then read. */
soft_i2c_status_t soft_i2c_mem_read(soft_i2c_t *bus, uint8_t addr7,
                                    uint8_t reg, uint8_t *data, size_t len);

soft_i2c_status_t soft_i2c_mem_write(soft_i2c_t *bus, uint8_t addr7,
                                     uint8_t reg, const uint8_t *data,
                                     size_t len);

#ifdef __cplusplus
}
#endif
#endif /* SOFT_I2C_H */