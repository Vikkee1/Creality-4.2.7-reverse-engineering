#ifndef EEPROM_BL23C16A_H
#define EEPROM_BL23C16A_H

#include <stdint.h>
#include "soft_i2c.h"

/* BL23C16A / 24C16: 2 KB EEPROM */
#define EEPROM_BASE_ADDR   0x50u
#define EEPROM_PAGE_SIZE   16u
#define EEPROM_WRITE_TIMEOUT_MS 20u

#endif