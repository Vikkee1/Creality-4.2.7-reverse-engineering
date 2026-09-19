#ifndef SOFT_I2C_STM32_H
#define SOFT_I2C_STM32_H

#include "stm32f1xx_hal.h"

#include "soft_i2c.h"

typedef struct {
    GPIO_TypeDef *sda_port;
    uint16_t      sda_pin;
    GPIO_TypeDef *scl_port;
    uint16_t      scl_pin;
} soft_i2c_stm32_ctx_t;

soft_i2c_status_t soft_i2c_stm32_init(soft_i2c_t *bus,
                                      soft_i2c_stm32_ctx_t *ctx,
                                      uint32_t half_period_us);
#endif