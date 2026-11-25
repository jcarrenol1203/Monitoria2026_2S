/*
 * stm32f4xx_it.c
 * Only SysTick lives here. Every other handler (TIM2, USART2, DMA1 streams)
 * is defined next to the code that owns it: heartbeat.c or the stage's app.c.
 */

#include "stm32f4xx_hal.h"

/* SysTick handler — required by HAL for HAL_Delay() and timeouts */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
