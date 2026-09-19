#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

/* USART1 is set up by CubeMX in main.c; huart1 is a plain (non-static)
 * global there, so it's reachable here without a shared header. */
extern UART_HandleTypeDef huart1;

void uart_log(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100u);
}

void uart_logf(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uart_log(buf);
}

void delay(uint32_t ms){
    HAL_Delay(ms);
}