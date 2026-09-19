#include "app.h"
#include "soft_i2c_stm32.h"
#include "i2c_bus_soft.h"
#include "eeprom_BL23C16A.h"

static soft_i2c_stm32_ctx_t s_i2c_ctx = {
                        .sda_port = GPIOA,
                        .sda_pin  = GPIO_PIN_11,
                        .scl_port = GPIOA,
                        .scl_pin  = GPIO_PIN_12 };

static soft_i2c_t s_i2c_bus;

int main_app(void)
{
    soft_i2c_status_t st = soft_i2c_stm32_init(&s_i2c_bus, &s_i2c_ctx, 5u);
    if (st != SOFT_I2C_OK) {
        uart_logf("soft_i2c_stm32_init failed: %d\r\n", (int)st);
        return -1;
    }

    i2c_bus_t eeprom_bus = soft_i2c_as_bus(&s_i2c_bus);
    eeprom_self_test(&eeprom_bus);

    return 0;
}
