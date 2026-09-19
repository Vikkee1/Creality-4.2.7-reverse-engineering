/*
 * i2c_bus.h — Generic transaction-level I2C interface.
 *
 * Mirrors soft_i2c_port_t one layer up: a function-pointer vtable plus a
 * void* ctx, so a device driver can call I2C transactions without knowing
 * whether they're backed by bit-banged GPIO or a real hardware peripheral.
 *
 * init/recover are deliberately not part of this interface - those are
 * transport-lifecycle concerns (bit-bang GPIO recovery vs. HAL error
 * handling share no common shape) and stay owned by whoever constructs
 * the concrete bus, before adapting it to this interface.
 */
#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2C_BUS_OK = 0,
    I2C_BUS_ERR_NACK_ADDR,
    I2C_BUS_ERR_NACK_DATA,
    I2C_BUS_ERR_TIMEOUT,
    I2C_BUS_ERR_BUS_BUSY,
    I2C_BUS_ERR_ARB_LOST,
    I2C_BUS_ERR_PARAM,
} i2c_status_t;

typedef struct {
    i2c_status_t (*probe)(void *ctx, uint8_t addr7);
    i2c_status_t (*write)(void *ctx, uint8_t addr7,
                           const uint8_t *data, size_t len);
    i2c_status_t (*read)(void *ctx, uint8_t addr7,
                          uint8_t *data, size_t len);
    i2c_status_t (*mem_write)(void *ctx, uint8_t addr7, uint8_t reg,
                               const uint8_t *data, size_t len);
    i2c_status_t (*mem_read)(void *ctx, uint8_t addr7, uint8_t reg,
                              uint8_t *data, size_t len);
    void *ctx;
} i2c_bus_t;

#ifdef __cplusplus
}
#endif
#endif /* I2C_BUS_H */
