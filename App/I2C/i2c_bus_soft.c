#include "i2c_bus_soft.h"

static i2c_status_t map_status(soft_i2c_status_t st)
{
    switch (st) {
    case SOFT_I2C_OK:            return I2C_BUS_OK;
    case SOFT_I2C_ERR_NACK_ADDR: return I2C_BUS_ERR_NACK_ADDR;
    case SOFT_I2C_ERR_NACK_DATA: return I2C_BUS_ERR_NACK_DATA;
    case SOFT_I2C_ERR_TIMEOUT:   return I2C_BUS_ERR_TIMEOUT;
    case SOFT_I2C_ERR_BUS_BUSY:  return I2C_BUS_ERR_BUS_BUSY;
    case SOFT_I2C_ERR_ARB_LOST:  return I2C_BUS_ERR_ARB_LOST;
    case SOFT_I2C_ERR_PARAM:
    default:                     return I2C_BUS_ERR_PARAM;
    }
}

static i2c_status_t soft_probe(void *bus, uint8_t addr7)
{
    return map_status(soft_i2c_probe((soft_i2c_t *)bus, addr7));
}

static i2c_status_t soft_write(void *bus, uint8_t addr7,
                                const uint8_t *data, size_t len)
{
    return map_status(soft_i2c_write((soft_i2c_t *)bus, addr7, data, len));
}

static i2c_status_t soft_read(void *bus, uint8_t addr7,
                               uint8_t *data, size_t len)
{
    return map_status(soft_i2c_read((soft_i2c_t *)bus, addr7, data, len));
}

static i2c_status_t soft_mem_write(void *bus, uint8_t addr7, uint8_t reg,
                                    const uint8_t *data, size_t len)
{
return map_status(soft_i2c_mem_write((soft_i2c_t *)bus, addr7, reg, data, len));
}

static i2c_status_t soft_mem_read(void *bus, uint8_t addr7, uint8_t reg,
                                   uint8_t *data, size_t len)
{
    return map_status(soft_i2c_mem_read((soft_i2c_t *)bus, addr7, reg, data, len));
}

i2c_bus_t soft_i2c_as_bus(soft_i2c_t *bus)
{
    i2c_bus_t b = {
        .probe     = soft_probe,
        .write     = soft_write,
        .read      = soft_read,
        .mem_write = soft_mem_write,
        .mem_read  = soft_mem_read,
        .ctx       = bus,
    };
    return b;
}
