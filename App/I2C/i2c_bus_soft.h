/*
 * i2c_bus_soft.h — Adapts soft_i2c_t onto the generic i2c_bus_t interface.
 *
 * The only files that know about both soft_i2c_t and i2c_bus_t.
 */
#ifndef I2C_BUS_SOFT_H
#define I2C_BUS_SOFT_H

#include "i2c_bus.h"
#include "soft_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Adapts an already-initialized soft_i2c_t into the generic i2c_bus_t
 * interface. `bus` must remain valid for as long as the returned
 * i2c_bus_t is used (same lifetime rule soft_i2c_port_t already has
 * relative to soft_i2c_t). */
i2c_bus_t soft_i2c_as_bus(soft_i2c_t *bus);

#ifdef __cplusplus
}
#endif
#endif /* I2C_BUS_SOFT_H */
