#ifndef HAL_H
#define HAL_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

void uart_log(const char *s);
void uart_logf(const char *fmt, ...);
void delay(uint32_t ms);

#endif