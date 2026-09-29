/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Taller de Electrónica Digital - Monitoría
 * @brief          : Punto de partida — Taller Semáforo con SysTick (CMSIS bare-metal)
 * @board          : STM32F411RE (NUCLEO-F411RE)
 ******************************************************************************
 */

#include <stm32f4xx.h>
#include <stdint.h>

int main(void)
{
    /* TODO: habilitar el reloj de los puertos GPIO que vas a usar (RCC->AHB1ENR) */

    /* TODO: configurar los pines de los 3 LEDs como salida */

    /* TODO: configurar el SysTick para que cuente 500 ms */

    while (1)
    {
        /* TODO: revisar el flag COUNTFLAG del SysTick y, cuando esté en 1,
         * avanzar el switch-case que cambia el color del semáforo */
    }
}
