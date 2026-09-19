/*
 * soft_i2c_stm32.c — STM32 adapter for the portable soft_i2c core.
 *
 * This is the ONLY file that includes a HAL header. Pins must be configured as
 * GPIO_MODE_OUTPUT_OD with external pull-ups.
 */
#include "soft_i2c_stm32.h"

/* --- Delay via the DWT cycle counter -------------------------------- */

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if defined(DWT_LAR_UNLOCK_REQUIRED)
    DWT->LAR = 0xC5ACCE55;      /* some Cortex-M7 parts need this */
#endif
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* --- Port callbacks -------------------------------------------------- */

static void stm32_sda_set(void *ctx, bool release)
{
    soft_i2c_stm32_ctx_t *c = (soft_i2c_stm32_ctx_t *)ctx;
    /* BSRR is atomic: low half sets, high half resets. No read-modify-write,
     * so an ISR touching another pin on this port can't clobber us. */
    c->sda_port->BSRR = release ? c->sda_pin : ((uint32_t)c->sda_pin << 16);
}

static void stm32_scl_set(void *ctx, bool release)
{
    soft_i2c_stm32_ctx_t *c = (soft_i2c_stm32_ctx_t *)ctx;
    c->scl_port->BSRR = release ? c->scl_pin : ((uint32_t)c->scl_pin << 16);
}

static bool stm32_sda_get(void *ctx)
{
    soft_i2c_stm32_ctx_t *c = (soft_i2c_stm32_ctx_t *)ctx;
    return (c->sda_port->IDR & c->sda_pin) != 0u;
}

static bool stm32_scl_get(void *ctx)
{
    soft_i2c_stm32_ctx_t *c = (soft_i2c_stm32_ctx_t *)ctx;
    return (c->scl_port->IDR & c->scl_pin) != 0u;
}

static void stm32_delay_us(void *ctx, uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000u);
    (void)ctx;
    while ((DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

/* --- Setup ----------------------------------------------------------- */

soft_i2c_status_t soft_i2c_stm32_init(soft_i2c_t *bus,
                                      soft_i2c_stm32_ctx_t *ctx,
                                      uint32_t half_period_us)
{
    GPIO_InitTypeDef gpio = {0};
    soft_i2c_port_t port;
    soft_i2c_status_t st;

    dwt_init();

    /* Preset the output latch high so the pins float rather than glitch
     * low the instant we switch them to output mode. */
    ctx->sda_port->BSRR = ctx->sda_pin;
    ctx->scl_port->BSRR = ctx->scl_pin;

    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_NOPULL;           /* rely on external pull-ups */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = ctx->sda_pin;
    HAL_GPIO_Init(ctx->sda_port, &gpio);
    gpio.Pin = ctx->scl_pin;
    HAL_GPIO_Init(ctx->scl_port, &gpio);

    port.sda_set  = stm32_sda_set;
    port.scl_set  = stm32_scl_set;
    port.sda_get  = stm32_sda_get;
    port.scl_get  = stm32_scl_get;
    port.delay_us = stm32_delay_us;
    port.ctx      = ctx;

    st = soft_i2c_init(bus, &port, half_period_us, 2000u);
    if (st != SOFT_I2C_OK) return st;

    return soft_i2c_recover(bus);
}